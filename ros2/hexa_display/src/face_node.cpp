// Hexapod "face" ROS2 node. Drives a 256x64 SH1122 OLED directly from the Pi,
// reusing the firmware's platform-free eye animation core (EyeAnim + EyeRaster)
// without the ESP-only Display/IRenderer/ExpressionController scaffolding.
//
// Robot state is auto-mapped to eye states elsewhere in the control stack; that
// node sets the target expression/gaze on this one. For manual and bring-up
// control the same states are exposed as String/Empty topics.

#include <chrono>
#include <cstdint>
#include <memory>
#include <random>
#include <string>

#include <rcl_interfaces/msg/parameter_descriptor.hpp>
#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/empty.hpp>
#include <std_msgs/msg/string.hpp>

#include "EyeAnim.h"
#include "EyeRaster.h"
#include "Expression.h"
#include "FaceNames.h"
#include "Sh1122Panel.h"
#include "u8g2.h"

namespace {

// EyeAnim's injected RNG. A process-wide mt19937, seeded once, satisfies the
// uint32_t(*)() signature (a plain function pointer cannot capture state).
uint32_t hostRand() {
    static std::mt19937 gen{std::random_device{}()};
    return gen();
}

void pixelSink(void* ctx, int x, int y) {
    u8g2_DrawPixel(static_cast<u8g2_t*>(ctx),
                   static_cast<u8g2_uint_t>(x), static_cast<u8g2_uint_t>(y));
}

// Exact equality is intended: drawEye is a pure function of these fields, so
// bit-identical frames rasterize to identical pixels. EyeAnim emits a constant
// frame while idle (lid == 1.0f, settled integer gaze), so this reliably fires.
bool sameFrame(const eyes::AnimFrame& a, const eyes::AnimFrame& b) {
    return a.expr == b.expr && a.lid == b.lid && a.gx == b.gx && a.gy == b.gy;
}

rcl_interfaces::msg::ParameterDescriptor readOnly(const std::string& desc) {
    rcl_interfaces::msg::ParameterDescriptor d;
    d.description = desc;
    d.read_only = true;
    return d;
}

}  // namespace

class FaceNode : public rclcpp::Node {
public:
    FaceNode() : Node("face_node"), _anim(&hostRand) {
        face::PanelConfig cfg;
        cfg.spi_device   = declare_parameter<std::string>(
            "spi_device", cfg.spi_device, readOnly("spidev path for the SH1122"));
        cfg.spi_speed_hz = static_cast<uint32_t>(declare_parameter<int>(
            "spi_speed_hz", static_cast<int>(cfg.spi_speed_hz),
            readOnly("SPI clock (Hz)")));
        cfg.gpio_chip = declare_parameter<std::string>(
            "gpio_chip", cfg.gpio_chip, readOnly("GPIO character device"));
        cfg.dc_line  = declare_parameter<int>("dc_line", 24, readOnly("DC GPIO line"));
        cfg.rst_line = declare_parameter<int>("rst_line", 25, readOnly("RST GPIO line"));
        cfg.cs_line  = declare_parameter<int>("cs_line", 8, readOnly("CS GPIO line, -1 to disable"));
        cfg.headless = declare_parameter<bool>(
            "headless", false, readOnly("run without touching SPI/GPIO"));

        const int hz = std::max(1, declare_parameter<int>(
            "render_hz", 60, readOnly("render loop rate (Hz)")));

        if (!_panel.begin(cfg)) {
            RCLCPP_FATAL(get_logger(),
                         "SH1122 init failed (spi=%s chip=%s). Check wiring, "
                         "dtparam=spi=on, and access to spidev/gpiochip "
                         "(groups spi/gpio), or set headless:=true.",
                         cfg.spi_device.c_str(), cfg.gpio_chip.c_str());
            throw std::runtime_error("panel init failed");
        }

        rclcpp::QoS cmd_qos(10);
        _expr_sub = create_subscription<std_msgs::msg::String>(
            "~/expression", cmd_qos,
            [this](const std_msgs::msg::String& m) { onExpression(m.data); });
        _gaze_sub = create_subscription<std_msgs::msg::String>(
            "~/gaze", cmd_qos,
            [this](const std_msgs::msg::String& m) { onGaze(m.data); });
        _blink_sub = create_subscription<std_msgs::msg::Empty>(
            "~/blink", cmd_qos,
            [this](const std_msgs::msg::Empty&) { _anim.requestBlink(); });

        // Latch the current state so late subscribers (e.g. a dashboard) get it.
        _state_pub = create_publisher<std_msgs::msg::String>(
            "~/current_state", rclcpp::QoS(1).transient_local());

        _render_timer = create_wall_timer(
            std::chrono::milliseconds(1000 / hz), [this] { renderTick(); });
        _state_timer = create_wall_timer(
            std::chrono::seconds(1), [this] { publishState(); });

        RCLCPP_INFO(get_logger(), "face up: 256x64 SH1122 on %s @ %d Hz%s",
                    cfg.spi_device.c_str(), hz, cfg.headless ? " (headless)" : "");
    }

    ~FaceNode() override { _panel.sleep(); }

private:
    uint32_t nowMs() {
        using namespace std::chrono;
        if (_t0.time_since_epoch().count() == 0) _t0 = steady_clock::now();
        return static_cast<uint32_t>(
            duration_cast<milliseconds>(steady_clock::now() - _t0).count());
    }

    void renderTick() {
        const eyes::AnimFrame f = _anim.update(_target, nowMs());
        // Skip the float-heavy raster entirely when the animation frame is
        // unchanged — between blinks and once gaze has settled the face is
        // static, so there is nothing to redraw. (present()'s dirty-flush still
        // guards SPI for sub-pixel frame changes that rasterize identically.)
        if (_haveFrame && sameFrame(f, _lastFrame)) return;
        _lastFrame = f;
        _haveFrame = true;

        _panel.clearBuffer();
        u8g2_t* g = _panel.u8g2();
        eyes::drawEye(f.expr, false, eyes::kEyeLX, f.lid, f.gx, f.gy, pixelSink, g);
        eyes::drawEye(f.expr, true,  eyes::kEyeRX, f.lid, f.gx, f.gy, pixelSink, g);
        _panel.present();  // flushes over SPI only if the pixels changed
    }

    void onExpression(const std::string& name) {
        if (auto e = face::parseExpression(name)) {
            _target.expr = *e;
        } else {
            RCLCPP_WARN(get_logger(), "unknown expression '%s'", name.c_str());
        }
    }

    void onGaze(const std::string& name) {
        if (auto gz = face::parseGaze(name)) {
            _target.gaze = *gz;
        } else {
            RCLCPP_WARN(get_logger(), "unknown gaze '%s'", name.c_str());
        }
    }

    void publishState() {
        std_msgs::msg::String m;
        m.data = std::string(expressionName(_target.expr)) + " " + gazeName(_target.gaze);
        _state_pub->publish(m);
    }

    face::Sh1122Panel _panel;
    eyes::EyeAnim     _anim;
    RenderState       _target{Expression::NEUTRAL, GazeDirection::CENTER};

    eyes::AnimFrame _lastFrame{};      // last frame rastered (raster-skip gate)
    bool            _haveFrame = false;

    std::chrono::steady_clock::time_point _t0{};

    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr _expr_sub;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr _gaze_sub;
    rclcpp::Subscription<std_msgs::msg::Empty>::SharedPtr  _blink_sub;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr    _state_pub;
    rclcpp::TimerBase::SharedPtr _render_timer;
    rclcpp::TimerBase::SharedPtr _state_timer;
};

int main(int argc, char** argv) {
    rclcpp::init(argc, argv);
    int rc = 0;
    try {
        rclcpp::spin(std::make_shared<FaceNode>());
    } catch (const std::exception& e) {
        RCLCPP_FATAL(rclcpp::get_logger("face_node"), "fatal: %s", e.what());
        rc = 1;
    }
    rclcpp::shutdown();
    return rc;
}
