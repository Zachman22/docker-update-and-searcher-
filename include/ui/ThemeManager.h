#pragma once

#include <QString>
#include <QApplication>
#include <QPalette>
#include <QStyle>

namespace ui {

/**
 * ThemeManager - Manages application themes including dark mode
 */
class ThemeManager {
public:
    enum class Theme {
        Light,
        Dark,
        System  // Follow system theme
    };

    /**
     * Apply theme to application
     * @param theme Theme to apply
     */
    static void applyTheme(Theme theme);

    /**
     * Get current theme
     * @return Current theme
     */
    static Theme currentTheme();

    /**
     * Check if dark mode is active
     * @return True if dark mode is active
     */
    static bool isDarkMode();

    /**
     * Get stylesheet for theme
     * @param theme Theme
     * @return Qt stylesheet string
     */
    static QString getStyleSheet(Theme theme);

    /**
     * Save theme preference
     * @param theme Theme to save
     */
    static void saveThemePreference(Theme theme);

    /**
     * Load theme preference
     * @return Saved theme preference
     */
    static Theme loadThemePreference();

private:
    static Theme currentTheme_;

    static QString getLightThemeStyleSheet();
    static QString getDarkThemeStyleSheet();
    static QPalette getDarkPalette();
    static QPalette getLightPalette();
};

} // namespace ui
