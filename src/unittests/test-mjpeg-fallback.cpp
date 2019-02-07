#define CATCH_CONFIG_MAIN
#include "spice-catch.hpp"

#include "mjpeg-fallback.hpp"

namespace ssa = spice::streaming_agent;


SCENARIO("test parsing mjpeg plugin options", "[mjpeg][options]") {
    GIVEN("A new mjpeg plugin") {
        ssa::MjpegPlugin plugin;

        WHEN("passing correct options") {
            std::vector<ssa::ConfigureOption> options = {
                {"framerate", "20"},
                {"mjpeg.quality", "90"},
                {NULL, NULL}
            };

            plugin.ParseOptions(options.data());
            ssa::MjpegSettings new_options = plugin.Options();

            THEN("the options are set in the plugin") {
                CHECK(new_options.fps == 20);
                CHECK(new_options.quality == 90);
            }
        }

        WHEN("passing an unknown option") {
            std::vector<ssa::ConfigureOption> options = {
                {"wakaka", "10"},
                {NULL, NULL}
            };

            THEN("ParseOptions should ignore the option") {
                REQUIRE_NOTHROW(
                    plugin.ParseOptions(options.data())
                );
            }
        }

        WHEN("passing an invalid option value") {
            std::vector<ssa::ConfigureOption> options = {
                {"framerate", "40"},
                {"mjpeg.quality", "toot"},
                {NULL, NULL}
            };

            THEN("ParseOptions throws an exception") {
                REQUIRE_THROWS_WITH(
                    plugin.ParseOptions(options.data()),
                    "Invalid value 'toot' for option 'mjpeg.quality'."
                );
            }
        }
    }
}
