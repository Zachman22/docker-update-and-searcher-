#include "ui/ThemeManager.h"
#include <QSettings>
#include <QColor>

namespace ui {

ThemeManager::Theme ThemeManager::currentTheme_ = Theme::Light;

void ThemeManager::applyTheme(Theme theme) {
    currentTheme_ = theme;

    QApplication* app = qobject_cast<QApplication*>(QApplication::instance());
    if (!app) return;

    switch (theme) {
        case Theme::Light:
            app->setPalette(getLightPalette());
            app->setStyleSheet(getLightThemeStyleSheet());
            break;

        case Theme::Dark:
            app->setPalette(getDarkPalette());
            app->setStyleSheet(getDarkThemeStyleSheet());
            break;

        case Theme::System:
            // Detect system theme and apply
            app->setPalette(QApplication::style()->standardPalette());
            app->setStyleSheet("");
            break;
    }
}

ThemeManager::Theme ThemeManager::currentTheme() {
    return currentTheme_;
}

bool ThemeManager::isDarkMode() {
    return currentTheme_ == Theme::Dark;
}

QString ThemeManager::getStyleSheet(Theme theme) {
    switch (theme) {
        case Theme::Dark:
            return getDarkThemeStyleSheet();
        case Theme::Light:
            return getLightThemeStyleSheet();
        default:
            return "";
    }
}

QString ThemeManager::getLightThemeStyleSheet() {
    return R"(
        QMainWindow {
            background-color: #f5f5f5;
        }

        QTableWidget {
            background-color: white;
            alternate-background-color: #f9f9f9;
            gridline-color: #e0e0e0;
        }

        QPushButton {
            background-color: #0078d4;
            color: white;
            border: none;
            padding: 6px 12px;
            border-radius: 4px;
        }

        QPushButton:hover {
            background-color: #106ebe;
        }

        QPushButton:pressed {
            background-color: #005a9e;
        }

        QPushButton:disabled {
            background-color: #cccccc;
            color: #666666;
        }

        QGroupBox {
            border: 1px solid #d0d0d0;
            border-radius: 4px;
            margin-top: 10px;
            padding-top: 10px;
        }

        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            padding: 0 5px;
        }
    )";
}

QString ThemeManager::getDarkThemeStyleSheet() {
    return R"(
        QMainWindow {
            background-color: #1e1e1e;
            color: #e0e0e0;
        }

        QWidget {
            background-color: #1e1e1e;
            color: #e0e0e0;
        }

        QTableWidget {
            background-color: #252525;
            alternate-background-color: #2d2d2d;
            gridline-color: #3a3a3a;
            color: #e0e0e0;
        }

        QTableWidget::item:selected {
            background-color: #0078d4;
        }

        QHeaderView::section {
            background-color: #2d2d2d;
            color: #e0e0e0;
            border: 1px solid #3a3a3a;
            padding: 4px;
        }

        QPushButton {
            background-color: #0078d4;
            color: white;
            border: none;
            padding: 6px 12px;
            border-radius: 4px;
        }

        QPushButton:hover {
            background-color: #106ebe;
        }

        QPushButton:pressed {
            background-color: #005a9e;
        }

        QPushButton:disabled {
            background-color: #3a3a3a;
            color: #888888;
        }

        QLineEdit, QTextEdit, QPlainTextEdit {
            background-color: #2d2d2d;
            color: #e0e0e0;
            border: 1px solid #3a3a3a;
            border-radius: 4px;
            padding: 4px;
        }

        QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus {
            border: 1px solid #0078d4;
        }

        QComboBox {
            background-color: #2d2d2d;
            color: #e0e0e0;
            border: 1px solid #3a3a3a;
            border-radius: 4px;
            padding: 4px;
        }

        QComboBox:hover {
            border: 1px solid #0078d4;
        }

        QComboBox::drop-down {
            border: none;
        }

        QComboBox QAbstractItemView {
            background-color: #2d2d2d;
            color: #e0e0e0;
            selection-background-color: #0078d4;
        }

        QGroupBox {
            border: 1px solid #3a3a3a;
            border-radius: 4px;
            margin-top: 10px;
            padding-top: 10px;
            color: #e0e0e0;
        }

        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            padding: 0 5px;
            color: #e0e0e0;
        }

        QTabWidget::pane {
            border: 1px solid #3a3a3a;
            background-color: #1e1e1e;
        }

        QTabBar::tab {
            background-color: #2d2d2d;
            color: #e0e0e0;
            border: 1px solid #3a3a3a;
            padding: 6px 12px;
        }

        QTabBar::tab:selected {
            background-color: #0078d4;
        }

        QTabBar::tab:hover {
            background-color: #3a3a3a;
        }

        QMenuBar {
            background-color: #2d2d2d;
            color: #e0e0e0;
        }

        QMenuBar::item:selected {
            background-color: #0078d4;
        }

        QMenu {
            background-color: #2d2d2d;
            color: #e0e0e0;
            border: 1px solid #3a3a3a;
        }

        QMenu::item:selected {
            background-color: #0078d4;
        }

        QStatusBar {
            background-color: #2d2d2d;
            color: #e0e0e0;
        }

        QToolBar {
            background-color: #2d2d2d;
            border: 1px solid #3a3a3a;
        }

        QScrollBar:vertical {
            background-color: #2d2d2d;
            width: 12px;
        }

        QScrollBar::handle:vertical {
            background-color: #3a3a3a;
            border-radius: 6px;
        }

        QScrollBar::handle:vertical:hover {
            background-color: #4a4a4a;
        }

        QScrollBar:horizontal {
            background-color: #2d2d2d;
            height: 12px;
        }

        QScrollBar::handle:horizontal {
            background-color: #3a3a3a;
            border-radius: 6px;
        }

        QScrollBar::handle:horizontal:hover {
            background-color: #4a4a4a;
        }
    )";
}

QPalette ThemeManager::getDarkPalette() {
    QPalette palette;

    palette.setColor(QPalette::Window, QColor(30, 30, 30));
    palette.setColor(QPalette::WindowText, QColor(224, 224, 224));
    palette.setColor(QPalette::Base, QColor(37, 37, 37));
    palette.setColor(QPalette::AlternateBase, QColor(45, 45, 45));
    palette.setColor(QPalette::ToolTipBase, QColor(224, 224, 224));
    palette.setColor(QPalette::ToolTipText, QColor(224, 224, 224));
    palette.setColor(QPalette::Text, QColor(224, 224, 224));
    palette.setColor(QPalette::Button, QColor(45, 45, 45));
    palette.setColor(QPalette::ButtonText, QColor(224, 224, 224));
    palette.setColor(QPalette::BrightText, Qt::white);
    palette.setColor(QPalette::Link, QColor(0, 120, 212));
    palette.setColor(QPalette::Highlight, QColor(0, 120, 212));
    palette.setColor(QPalette::HighlightedText, Qt::white);

    return palette;
}

QPalette ThemeManager::getLightPalette() {
    return QApplication::style()->standardPalette();
}

void ThemeManager::saveThemePreference(Theme theme) {
    QSettings settings("DockerHomelabManager", "Settings");
    settings.setValue("theme", static_cast<int>(theme));
}

ThemeManager::Theme ThemeManager::loadThemePreference() {
    QSettings settings("DockerHomelabManager", "Settings");
    int themeInt = settings.value("theme", static_cast<int>(Theme::Light)).toInt();
    return static_cast<Theme>(themeInt);
}

} // namespace ui
