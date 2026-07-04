#include "sdl_painter/app/app_config.h"
#include "sdl_painter/app/events.h"

#include <gtest/gtest.h>

namespace sdl_painter {

TEST(AppConfigTest, Defaults) {
  AppConfig cfg;
  EXPECT_EQ(cfg.title, "SDLPainter Application");
  EXPECT_EQ(cfg.width, 800);
  EXPECT_EQ(cfg.height, 600);
  EXPECT_TRUE(cfg.resizable);
  EXPECT_TRUE(cfg.vsync);
  EXPECT_EQ(cfg.msaa_samples, 4);
  EXPECT_EQ(cfg.backend, RendererBackend::kOpenGL);
  EXPECT_TRUE(cfg.init_logger);
}

TEST(AppConfigTest, FieldAssignment) {
  AppConfig cfg;
  cfg.title = "Test";
  cfg.width = 1024;
  cfg.height = 768;
  cfg.backend = RendererBackend::kVulkan;
  EXPECT_EQ(cfg.title, "Test");
  EXPECT_EQ(cfg.width, 1024);
  EXPECT_EQ(cfg.height, 768);
  EXPECT_EQ(cfg.backend, RendererBackend::kVulkan);
}

TEST(KeyModifierTest, OrCombinesFlags) {
  const KeyModifier mods = KeyModifier::kShift | KeyModifier::kCtrl;
  EXPECT_TRUE(HasModifier(mods, KeyModifier::kShift));
  EXPECT_TRUE(HasModifier(mods, KeyModifier::kCtrl));
  EXPECT_FALSE(HasModifier(mods, KeyModifier::kAlt));
  EXPECT_FALSE(HasModifier(mods, KeyModifier::kGui));
}

TEST(KeyModifierTest, NoneHasNoModifiers) {
  EXPECT_FALSE(HasModifier(KeyModifier::kNone, KeyModifier::kShift));
  EXPECT_FALSE(HasModifier(KeyModifier::kNone, KeyModifier::kNone));
}

}  // namespace sdl_painter
