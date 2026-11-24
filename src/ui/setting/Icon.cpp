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
        const QImage mark(QStringLiteral(":/Throne/Throne.png"));
        if (status == Icon::TrayIconStatus::Vpn) return QIcon(QPixmap::fromImage(mark));
        QColor fill;
        switch (status) {
            case Icon::TrayIconStatus::None:        fill = QColor("#a6a9ad"); break;
            case Icon::TrayIconStatus::Connecting:  fill = QColor("#ffc34d"); break;
            case Icon::TrayIconStatus::Running:     fill = QColor("#70daa1"); break;
            case Icon::TrayIconStatus::SystemProxy: fill = QColor("#81caff"); break;
            case Icon::TrayIconStatus::Vpn:         break;
        }
        QImage tinted = mark.convertToFormat(QImage::Format_ARGB32);
        for (int y=0; y<tinted.height(); ++y) {
            auto *line = reinterpret_cast<QRgb *>(tinted.scanLine(y));
            for (int x=0; x<tinted.width(); ++x) {
                const QRgb pixel = line[x];
                // Green-channel coverage separates the lime face from the two dark strokes.
                const qreal weight = qBound(0.0,(qGreen(pixel)-37)/209.0,1.0);
                const auto blend = [weight](int dark,int light) { return qRound(dark+(light-dark)*weight); };
                if (qAlpha(pixel)) line[x]=qRgba(blend(32,fill.red()),blend(37,fill.green()),
                                                blend(31,fill.blue()),qAlpha(pixel));
            }
        }
        return QIcon(QPixmap::fromImage(tinted));
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
