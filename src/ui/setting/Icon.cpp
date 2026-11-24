#include "include/ui/setting/Icon.hpp"

#include "include/global/Configs.hpp"

#include <QHash>
#include <QPainter>
#include <QPixmap>

#include <optional>

namespace {
    QHash<Icon::TrayIconStatus, QIcon> g_trayIcons;
    std::optional<bool> g_trayIconsCustom;

    QString statusName(Icon::TrayIconStatus status) {
        switch (status) {
            case Icon::TrayIconStatus::None: return QStringLiteral("Off");
            case Icon::TrayIconStatus::Connecting: return QStringLiteral("Connecting");
            case Icon::TrayIconStatus::Running: return QStringLiteral("Throne");
            case Icon::TrayIconStatus::SystemProxy: return QStringLiteral("Proxy");
            case Icon::TrayIconStatus::Vpn: return QStringLiteral("Tun");
        }
        MW_show_log("Icon::GetTrayIcon: Unknown status");
        return QStringLiteral("Off");
    }

    QIcon bundledIcon(Icon::TrayIconStatus status) {
        QPixmap logo(QStringLiteral(":/Throne/Throne.png"));
        QColor indicator;
        switch (status) {
            case Icon::TrayIconStatus::None:        indicator = QColor(0xb3, 0xb8, 0xb0); break;
            case Icon::TrayIconStatus::Connecting:  indicator = QColor(0xff, 0xc3, 0x46); break;
            case Icon::TrayIconStatus::Running:     indicator = QColor(0x6e, 0xd8, 0x99); break;
            case Icon::TrayIconStatus::SystemProxy: indicator = QColor(0x79, 0xc9, 0xff); break;
            case Icon::TrayIconStatus::Vpn:         return QIcon(logo);
        }
        QPainter painter(&logo);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setWindow(0, 0, 32, 32);
        painter.setPen(QPen(QColor(0x20, 0x25, 0x1f), 1.4));
        painter.setBrush(indicator);
        painter.drawEllipse(QPointF(25.7, 25.5), 4.5, 4.5);
        painter.end();
        return QIcon(logo);
    }

    QIcon loadNamedIcon(Icon::TrayIconStatus status, bool useCustom) {
        if (useCustom) {
            const QString name = statusName(status);
            // Older custom sets may only provide Off.png for the transition state.
            const QString customFallback = status == Icon::TrayIconStatus::Connecting ? statusName(Icon::TrayIconStatus::None) : QString();
            for (const auto &candidate : {name, customFallback}) {
                if (candidate.isEmpty()) continue;
                // QIcon(path).isNull() is not a decode check: a PNG with a valid signature and a corrupt body passes it.
                if (const auto custom = QPixmap(QStringLiteral("icons/") + candidate + QStringLiteral(".png")); !custom.isNull()) return QIcon(custom);
            }
        }
        return bundledIcon(status);
    }
} // namespace

void Icon::InvalidateTrayIconCache() {
    g_trayIcons.clear();
}

QIcon Icon::GetTrayIcon(TrayIconStatus status) {
    const bool useCustom = Configs::dataManager->settingsRepo->use_custom_icons;
    if (g_trayIconsCustom != useCustom) {
        g_trayIcons.clear();
        g_trayIconsCustom = useCustom;
    }
    if (const auto it = g_trayIcons.constFind(status); it != g_trayIcons.cend()) return it.value();

    const QIcon icon = loadNamedIcon(status, useCustom);
    g_trayIcons.insert(status, icon);
    return icon;
}

QIcon Icon::GetTaskbarIcon(TrayIconStatus status) {
    const auto &settings = Configs::dataManager->settingsRepo;
    // The bundled icon only: a custom PNG has no room for the padding macOS adds in the dock.
    if (settings->use_custom_icons && !settings->follow_status_in_taskbar) {
        return QIcon(QStringLiteral(":/Throne/Throne.png"));
    }
    return GetTrayIcon(status);
}
