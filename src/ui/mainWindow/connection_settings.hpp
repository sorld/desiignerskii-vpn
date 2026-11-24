#pragma once

#include "include/database/RoutesRepo.h"
#include "include/engine/connection/ConnectionManager.h"
#include "include/engine/services/SettingsRegistry.h"
#include "include/global/Configs.hpp"
#include "include/sys/AutoRun.hpp"
#include "include/ui/setting/ThemeManager.hpp"

#include <functional>
#include <QApplication>
#include <QPainter>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

// The desktop and the control API use the same validators and persistence for engine settings.
class ConnectionSettings final : public QDialog {
public:
    enum { OpenRoutes = 2 };
    ConnectionSettings(QWidget *parent, Engine::ConnectionManager *engine)
        : QDialog(parent), connection(engine), original(Engine::SettingsRegistry::Values()) {
        setObjectName("connectionSettings");
        setWindowTitle("Настройки · Desiignerskii VPN");
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(16,16,16,14);
        auto *body = new QHBoxLayout;
        body->setSpacing(18);
        nav = new QListWidget(this);
        nav->setObjectName("sections");
        nav->setAccessibleName("Разделы настроек");
        nav->setFixedWidth(146);
        pages = new QStackedWidget(this);
        body->addWidget(nav);
        body->addWidget(pages,1);
        layout->addLayout(body,1);
        const auto &cfg = Configs::dataManager->settingsRepo;
        section("Подключение");
        mode = select("Режим подключения");
        mode->addItem("Локальный прокси",0);
        mode->addItem("Полный туннель (TUN)",1);
        mode->addItem("Системный прокси",2);
        mode->addItem("TUN и системный прокси",3);
        initialMode = (cfg->spmode_vpn ? 1 : 0) | (cfg->spmode_system_proxy ? 2 : 0);
        mode->setCurrentIndex(mode->findData(initialMode));
        flag("remember_enable","Восстанавливать подключение при запуске");
        flag("tun_ipv6","IPv6 в туннеле");
        flag("tun_strict_route","Строгая маршрутизация");
        number("tun_mtu","MTU туннеля",1000,10000);
        field("tun_ipv4_cidr","IPv4-адрес туннеля");
        field("tun_ipv6_cidr","IPv6-адрес туннеля");

        section("DNS");
        field("remote_dns","Через VPN");
        field("direct_dns","При прямом подключении");
        note("Это общие DNS-серверы. Профиль маршрутов может задавать собственные адреса.");

        section("Маршруты");
        route = select("Профиль по умолчанию");
        for (const auto &item : Configs::dataManager->routesRepo->GetAllRouteProfiles()) {
            if (item) route->addItem(item->name,item->id);
        }
        initialRoute = cfg->current_route_id;
        route->setCurrentIndex(route->findData(initialRoute));
        interval("route_auto_update","Обновление удалённых маршрутов");
        auto *rules = new QPushButton("Правила для сайтов и приложений…");
        form->addRow(rules);
        connect(rules,&QPushButton::clicked,this,[this] {
            if (dirty()) {
                auto answer = QMessageBox::question(this,"Настройки","Сохранить изменения перед открытием правил?",
                    QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,QMessageBox::Cancel);
                if (answer == QMessageBox::Cancel || (answer == QMessageBox::Save && !save())) return;
            }
            done(OpenRoutes);
        });

        section("Подписки");
        interval("sub_auto_update","Автообновление");
        flag("sub_respect_server_interval","Использовать интервал провайдера");
        hwid = toggle("Отправлять провайдеру ID устройства",cfg->sub_send_hwid);
        notify = toggle("Показывать изменения после обновления",cfg->sub_show_change_popup);
        initialHwid = cfg->sub_send_hwid;
        initialNotify = cfg->sub_show_change_popup;

        section("Приложение");
        theme = select("Оформление");
        theme->addItem("Системное","System");
        theme->addItem("Тёмное","QDarkStyle");
        theme->addItem("Светлое","LightBlue");
        initialTheme = cfg->theme;
        if (theme->findData(initialTheme) < 0) theme->addItem(initialTheme,initialTheme);
        theme->setCurrentIndex(theme->findData(initialTheme));
        initialAutoRun = AutoRun_IsEnabled();
        autoRun = toggle("Запускать при входе в систему",initialAutoRun);
        initialTray = cfg->start_minimal;
        startTray = toggle("Открывать свёрнутым в трей",initialTray);

        section("Диагностика");
        field("test_url","Адрес проверки задержки");
        number("test_timeout_ms","Тайм-аут проверки, мс",0,60000);
        number("test_concurrency","Параллельные проверки",1,499);
        auto *level = select("Уровень журнала");
        for (const auto &name : QStringList{"trace","debug","info","warn","error","fatal","panic"}) level->addItem(name,name);
        level->setCurrentIndex(level->findData(original.value("log_level").toString()));
        track("log_level",level,[level]{return level->currentData().toString();});

        connect(nav,&QListWidget::currentRowChanged,pages,&QStackedWidget::setCurrentIndex);
        nav->setCurrentRow(0);
        error = new QLabel(this);
        error->setTextFormat(Qt::PlainText);
        error->setWordWrap(true);
        error->setObjectName("validation");
        error->hide();
        layout->addWidget(error);
        auto *footer = new QHBoxLayout;
        footer->addStretch();
        auto *cancel = new QPushButton("Отмена",this);
        auto *apply = new QPushButton("Сохранить",this);
        apply->setObjectName("saveSettings");
        apply->setDefault(true);
        footer->addWidget(cancel);
        footer->addWidget(apply);
        layout->addLayout(footer);
        connect(cancel,&QPushButton::clicked,this,&QDialog::reject);
        connect(apply,&QPushButton::clicked,this,[this] { if (save()) accept(); });
        styleDialog();
        connect(themeManager(),&ThemeManager::themeChanged,this,[this]{ styleDialog(); });
        resize(QSize(670,505).boundedTo(parent->screen()->availableGeometry().size()-QSize(20,48)));
    }

private:
    class Switch final : public QCheckBox {
    public:
        explicit Switch(const QString &label) : QCheckBox(label) {
            setMinimumHeight(28);
            setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
            setCursor(Qt::PointingHandCursor);
        }
        QSize sizeHint() const override { return QSize(fontMetrics().horizontalAdvance(text())+50,30); }
    protected:
        bool hitButton(const QPoint &pos) const override { return rect().contains(pos); }
        void paintEvent(QPaintEvent *) override {
            const bool dark = palette().color(QPalette::Window).lightness()<128;
            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setPen(dark?QColor("#edf0ed"):QColor("#1d231e"));
            const QRect words = rect().adjusted(0,0,-46,0);
            painter.drawText(words,Qt::AlignLeft|Qt::AlignVCenter,
                fontMetrics().elidedText(text(),Qt::ElideRight,words.width()));
            const QRectF rail(width()-36,(height()-18)/2.0,34,18);
            painter.setPen(Qt::NoPen);
            painter.setBrush(isChecked()?QColor("#bcea78"):(dark?QColor("#505b54"):QColor("#b4bcb6")));
            painter.drawRoundedRect(rail,9,9);
            painter.setBrush(isChecked()?QColor("#253022"):QColor("#ffffff"));
            painter.drawEllipse(QPointF(isChecked()?rail.right()-9:rail.left()+9,rail.center().y()),6,6);
            if (hasFocus() && testAttribute(Qt::WA_KeyboardFocusChange)) {
                painter.setPen(QPen(dark?QColor("#bcea78"):QColor("#567342"),1,Qt::DashLine));
                painter.setBrush(Qt::NoBrush);
                painter.drawRoundedRect(rail.adjusted(-1,-2,1,2),10,10);
            }
        }
    };
    struct Setting {
        QString key;
        QWidget *editor;
        int page;
        std::function<QJsonValue()> read;
    };
    Engine::ConnectionManager *connection;
    QJsonObject original;
    QList<Setting> controls;
    QListWidget *nav;
    QStackedWidget *pages;
    QFormLayout *form;
    QComboBox *mode, *theme, *route;
    QCheckBox *hwid, *notify, *startTray, *autoRun;
    QLabel *error;
    int initialMode, initialRoute;
    bool initialHwid, initialNotify, initialTray, initialAutoRun;
    QString initialTheme;

    void section(const QString &title) {
        nav->addItem(title);
        auto *scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        auto *page = new QWidget;
        page->setObjectName("settingsPage");
        auto *column = new QVBoxLayout(page);
        column->setContentsMargins(4,4,8,10);
        column->setSpacing(16);
        auto *heading = new QLabel(title);
        heading->setObjectName("sectionTitle");
        column->addWidget(heading);
        form = new QFormLayout;
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        form->setRowWrapPolicy(QFormLayout::WrapAllRows);
        form->setVerticalSpacing(8);
        column->addLayout(form);
        column->addStretch();
        scroll->setWidget(page);
        pages->addWidget(scroll);
    }

    void track(const QString &key,QWidget *editor,std::function<QJsonValue()> read) {
        editor->setObjectName(key);
        controls.append({key,editor,pages->count()-1,std::move(read)});
    }

    QComboBox *select(const QString &label) {
        auto *box = new QComboBox;
        box->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        box->setAccessibleName(label);
        form->addRow(label,box);
        return box;
    }

    QCheckBox *toggle(const QString &title,bool initial) {
        auto *box = new Switch(title);
        box->setAccessibleName(title);
        box->setChecked(initial);
        form->addRow(box);
        return box;
    }

    void flag(const QString &key,const QString &title) {
        auto *box = toggle(title,original.value(key).toBool());
        track(key,box,[box]{return box->isChecked();});
    }

    void number(const QString &key,const QString &title,int low,int high) {
        auto *box = new QSpinBox;
        const int value = original.value(key).toInt();
        box->setRange(qMin(low,value),qMax(high,value)); // Keep legacy values until explicitly edited.
        box->setValue(value);
        box->setAccessibleName(title);
        form->addRow(title,box);
        track(key,box,[box]{return box->value();});
    }

    void field(const QString &key,const QString &title) {
        auto *edit = new QLineEdit(original.value(key).toString());
        edit->setAccessibleName(title);
        form->addRow(title,edit);
        track(key,edit,[edit]{return edit->text().trimmed();});
    }

    void interval(const QString &key,const QString &title) {
        auto *box = select(title);
        box->addItem("Выключено",0);
        box->addItem("Каждые 30 минут",30);
        box->addItem("Каждый час",60);
        box->addItem("Каждые 6 часов",360);
        box->addItem("Каждые 12 часов",720);
        box->addItem("Раз в сутки",1440);
        const auto minutes = original.value(key).toInt();
        if (box->findData(minutes)<0) box->addItem(QString("%1 мин").arg(minutes),minutes);
        box->setCurrentIndex(box->findData(minutes));
        track(key,box,[box]{return box->currentData().toInt();});
    }

    void note(const QString &value) {
        auto *label = new QLabel(value);
        label->setObjectName("note");
        label->setWordWrap(true);
        form->addRow(label);
    }

    QJsonObject changes() const {
        QJsonObject patch;
        for (const auto &item : controls) {
            const auto value = item.read();
            if (value != original.value(item.key)) patch.insert(item.key,value);
        }
        return patch;
    }

    bool dirty() const {
        return !changes().isEmpty() || mode->currentData().toInt()!=initialMode ||
            (route->currentIndex()>=0 && route->currentData().toInt()!=initialRoute) ||
            hwid->isChecked()!=initialHwid || notify->isChecked()!=initialNotify ||
            startTray->isChecked()!=initialTray || autoRun->isChecked()!=initialAutoRun ||
            theme->currentData().toString()!=initialTheme;
    }

    bool fail(const QString &message,int page,QWidget *control) {
        nav->setCurrentRow(page);
        control->setFocus();
        error->setText(message);
        error->show();
        return false;
    }

    bool save() {
        auto &cfg = *Configs::dataManager->settingsRepo;
        if (!dirty()) return true;
        const auto patch = changes();
        const bool ipv6 = patch.value("tun_ipv6").toBool(cfg.vpn_ipv6);
        const int mtu = patch.value("tun_mtu").toInt(cfg.vpn_mtu);
        if ((patch.contains("tun_mtu") || patch.contains("tun_ipv6")) && ipv6 && mtu<1280)
            return fail("Для IPv6 укажи MTU от 1280.",0,findChild<QSpinBox*>("tun_mtu"));
        const int chosenMode = mode->currentData().toInt();
        if (chosenMode!=initialMode && (chosenMode & 2) && cfg.disable_mixed_inbound)
            return fail("Для системного прокси включи входящий SOCKS/HTTP в расширенных настройках.",0,mode);
        const int routeId = route->currentData().toInt();
        if (route->currentIndex()>=0 && routeId!=initialRoute && !Configs::dataManager->routesRepo->HasRouteProfile(routeId))
            return fail("Этот профиль маршрутов был удалён. Выбери другой.",2,route);
        Engine::SettingsRegistry::PutResult result;
        if (!patch.isEmpty()) {
            const Engine::SettingsRegistry::Context context{bool(connection->runningSnapshot()),cfg.spmode_vpn,cfg.spmode_system_proxy};
            if (const auto problem = Engine::SettingsRegistry::Put(patch,context,&result)) {
                for (const auto &item : controls) {
                    if (problem->message.contains('"'+item.key+'"'))
                        return fail("Проверь значение поля «"+item.editor->accessibleName()+"».",item.page,item.editor);
                }
                return fail(problem->message,nav->currentRow(),nav);
            }
            for (auto it=patch.begin(); it!=patch.end(); ++it) original.insert(it.key(),it.value());
        }
        if (hwid->isChecked()!=initialHwid) cfg.sub_send_hwid = initialHwid = hwid->isChecked();
        if (notify->isChecked()!=initialNotify) cfg.sub_show_change_popup = initialNotify = notify->isChecked();
        if (startTray->isChecked()!=initialTray) cfg.start_minimal = initialTray = startTray->isChecked();
        if (theme->currentData().toString()!=initialTheme) {
            cfg.theme = initialTheme = theme->currentData().toString();
            themeManager()->ApplyTheme(cfg.theme);
        }
        cfg.Save();
        if (result.restartNeeded) connection->noteRestartNeeded(Engine::RestartReason::Settings);
        if (route->currentIndex()>=0 && routeId!=initialRoute) {
            connection->chooseRoute(routeId);
            initialRoute = cfg.current_route_id;
        }
        if (chosenMode!=initialMode) {
            // Gain the requested transport before releasing the previous one. Elevation may be declined.
            const bool tun = chosenMode & 1, proxy = chosenMode & 2;
            if (proxy && !cfg.spmode_system_proxy) connection->setSystemProxy(true);
            if (tun != cfg.spmode_vpn) connection->setTun(tun);
            if (tun==cfg.spmode_vpn && !proxy && cfg.spmode_system_proxy) connection->setSystemProxy(false);
            initialMode = (cfg.spmode_vpn?1:0) | (cfg.spmode_system_proxy?2:0);
            if (chosenMode!=initialMode) {
                mode->setCurrentIndex(mode->findData(initialMode));
                return fail("Режим подключения не изменён полностью. Показан текущий режим.",0,mode);
            }
        }
        if (autoRun->isChecked()!=initialAutoRun) {
            QString reason;
            if (!AutoRun_SetEnabled(autoRun->isChecked(),&reason)) return fail(reason,4,autoRun);
            initialAutoRun = autoRun->isChecked();
        }
        return true;
    }

    void styleDialog() {
        const bool dark = qApp->palette().color(QPalette::Window).lightness()<128;
        const QString bg = dark?"#191d1a":"#ffffff", fg = dark?"#edf0ed":"#1d231e";
        const QString well = dark?"#292f2b":"#f0f3ef", line = dark?"#465047":"#cbd3cb";
        const QString muted = dark?"#bac3b9":"#576358";
        setStyleSheet(QStringLiteral(R"(
          QDialog#connectionSettings { background:%1; color:%2; }
          #connectionSettings QWidget { color:%2; font:14px 'Segoe UI'; }
          #connectionSettings QScrollArea, #connectionSettings QWidget#settingsPage,
          #connectionSettings QStackedWidget { background:%1; border:0; }
          #connectionSettings QListWidget { background:%1; border:0; outline:0; }
          #connectionSettings QListWidget::item { padding:9px 6px; border-radius:7px; }
          #connectionSettings QListWidget::item:selected { background:%3; color:%2; }
          #connectionSettings QLabel#sectionTitle { font-size:23px; font-weight:650; }
          #connectionSettings QLabel#note { color:%5; }
          #connectionSettings QLabel#validation { color:%2; background:%3; padding:6px; }
          #connectionSettings QLineEdit, #connectionSettings QComboBox, #connectionSettings QSpinBox {
              padding:5px 8px; min-height:21px; background:%3; border:1px solid %4; border-radius:6px; }
          #connectionSettings QPushButton { background:%3; border:1px solid %4; border-radius:7px; padding:7px 12px; }
          #connectionSettings QPushButton#saveSettings { background:#cdf479; color:#1d271b; border:1px solid #94ba54; }
          #connectionSettings QCheckBox { spacing:8px; padding:3px 0; }
          #connectionSettings QLineEdit:focus, #connectionSettings QComboBox:focus,
          #connectionSettings QSpinBox:focus,#connectionSettings QPushButton:focus { border:1px solid %5; }
        )").arg(bg,fg,well,line,muted));
    }
};
