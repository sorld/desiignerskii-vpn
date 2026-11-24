#include "include/ui/mainwindow.h"
#include "connection_visuals.hpp"
#include "include/ui/setting/ThemeManager.hpp"

#include <QScrollArea>
#include "include/configs/sub/GroupUpdater.hpp"
#include "include/database/GroupsRepo.h"
#include "include/database/ProfilesRepo.h"
#include "include/engine/EngineEvents.h"
#include "include/engine/connection/ConnectionManager.h"

#include <QComboBox>
#include <QDir>
#include <QApplication>
#include <QHBoxLayout>
#include <QSignalBlocker>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace {

class ConnectionHome final : public QWidget {
public:
    explicit ConnectionHome(QWidget *parent) : QWidget(parent) {
        setObjectName("connectionHome");
        setAttribute(Qt::WA_StyledBackground,true);
        auto *outer = new QVBoxLayout(this);
        outer->setContentsMargins(0,0,0,0);
        outer->setSpacing(0);
        auto *bar=new QWidget(this);
        bar->setObjectName("appBar");
        auto *barLayout=new QHBoxLayout(bar);
        barLayout->setContentsMargins(20,0,20,0);
        auto *head=new QWidget(bar);
        head->setMaximumWidth(832);
        auto *header=new QHBoxLayout(head);
        header->setContentsMargins(0,12,0,12);
        header->setSpacing(8);
        auto *mark=new QLabel(head);
        mark->setPixmap(QIcon(":/Throne/Throne.png").pixmap(QSize(30,30),devicePixelRatioF()));
        header->addWidget(mark);
        header->addWidget(text("Desiignerskii VPN","wordmark")); header->addStretch();
        preferences=button({},header);
        preferences->setAccessibleName("Настройки"); preferences->setToolTip("Настройки"); preferences->setFixedSize(36,36);
        advanced=button("Расширенный вид",header);
        barLayout->addStretch(); barLayout->addWidget(head,100); barLayout->addStretch();
        outer->addWidget(bar);

        auto *scroll=new QScrollArea(this);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidgetResizable(true); scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        auto *page=new QWidget(scroll);
        auto *alignment=new QHBoxLayout(page);
        alignment->setContentsMargins(20,0,20,0); alignment->setSpacing(0);
        auto *column=new QWidget(page); column->setMaximumWidth(832);
        alignment->addStretch(); alignment->addWidget(column,100); alignment->addStretch();
        auto *body=new QVBoxLayout(column);
        body->setContentsMargins(0,24,0,22); body->setSpacing(0);
        banner=new ConnectionVisuals::Banner(column);
        banner->setMinimumHeight(208);
        auto *hero=new QVBoxLayout(banner);
        hero->setContentsMargins(26,20,26,22); hero->setSpacing(0);
        mode=text({},"mode");
        hero->addWidget(mode);
        hero->addSpacing(9);
        state=text({},"state");
        hero->addWidget(state);
        detail=text({},"location");
        detail->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
        hero->addWidget(detail);
        hero->addStretch();
        auto *actionRow=new QHBoxLayout;
        power=button("Подключить",actionRow); power->setObjectName("power");
        actionRow->addStretch(); hero->addLayout(actionRow);
        body->addWidget(banner);
        stats=new QWidget(column); auto *numbers=new QHBoxLayout(stats);
        numbers->setContentsMargins(2,14,2,13); numbers->setSpacing(8);
        numbers->addWidget(text("Принято","statLabel"));
        down=text({},"statValue"); numbers->addWidget(down);
        numbers->addSpacing(18);
        numbers->addWidget(text("Отправлено","statLabel"));
        up=text({},"statValue"); numbers->addWidget(up);
        numbers->addStretch(); body->addWidget(stats);
        body->addSpacing(18);
        auto *titleRow=new QHBoxLayout;
        titleRow->addWidget(text("Серверы","heading")); titleRow->addStretch();
        add=button("Добавить подписку",titleRow);
        body->addLayout(titleRow);
        body->addSpacing(11);
        auto *filterRow=new QHBoxLayout;
        groups=new ConnectionVisuals::SubscriptionPicker(column);
        groups->setAccessibleName("Подписка");
        groups->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        groups->setMinimumContentsLength(12);
        groups->setSizePolicy(QSizePolicy::Maximum,QSizePolicy::Fixed);
        groups->setMaximumWidth(420);
        filterRow->addWidget(groups);
        refresh=button({},filterRow);
        refresh->setFixedSize(34,34); refresh->setAccessibleName("Обновить подписку"); refresh->setToolTip("Обновить подписку");
        filterRow->addStretch(); body->addLayout(filterRow); body->addSpacing(8);
        servers=new QListWidget(column);
        servers->setObjectName("serverEntries");
        servers->setAccessibleName("Серверы");
        servers->setItemDelegate(new ConnectionVisuals::ServerRows(servers));
        servers->setFrameShape(QFrame::NoFrame); servers->setMouseTracking(true);
        servers->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        servers->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        servers->setUniformItemSizes(true);
        body->addWidget(servers);
        empty=text({},"muted"); empty->setWordWrap(true);
        body->addWidget(empty);
        body->addStretch(1);
        scroll->setWidget(page); outer->addWidget(scroll,1);
        settings=new QSettings(QDir::current().absoluteFilePath("desiignerskii-ui.ini"),QSettings::IniFormat,this);
        connect(themeManager(),&ThemeManager::themeChanged,this,[this] { paintHome(); });
        paintHome();
    }

    void paintHome() {
        const bool dark=qApp->palette().color(QPalette::Window).lightness()<128;
        const QString bg=dark?"#181c1a":"#fafbf9", ink=dark?"#eff2ef":"#202622";
        const QString muted=dark?"#a7b1aa":"#5c6860", line=dark?"#303732":"#e0e5de";
        const QString well=dark?"#292e2a":"#eaeee9", highlight=dark?"#374339":"#dde8d9";
        setStyleSheet(QStringLiteral(R"(
            QWidget#connectionHome, #connectionHome QScrollArea, #connectionHome QScrollArea > QWidget {background:%1;border:0;}
            #connectionHome QWidget {background:transparent;color:%2;}
            #connectionHome QWidget#appBar {background:%1;border:0;border-bottom:1px solid %4;}
            #connectionHome QLabel {border:0;font:14px 'Segoe UI';padding:0;margin:0;}
            #connectionHome QLabel#wordmark {font-size:17px;font-weight:700;}
            #connectionHome QLabel#mode {color:#c4d4c3;font-size:13px;font-weight:600;}
            #connectionHome QLabel#state {color:#fafdf8;font-size:36px;font-weight:650;}
            #connectionHome QLabel#location {color:#d2dbd2;font-size:15px;}
            #connectionHome QLabel#heading {font-size:24px;font-weight:650;}
            #connectionHome QLabel#muted,#connectionHome QLabel#statLabel {color:%3;font-size:13px;}
            #connectionHome QLabel#statValue {color:%2;font-size:14px;font-weight:600;}
            #connectionHome QPushButton,#connectionHome QComboBox {background:%5;color:%2;border:1px solid transparent;border-radius:9px;
                padding:6px 12px;font:13px 'Segoe UI';min-height:19px;}
            #connectionHome QComboBox {padding:5px 24px 5px 11px;}
            #connectionHome QComboBox::drop-down {subcontrol-origin:border;subcontrol-position:center right;width:22px;border:0;background:transparent;}
            #connectionHome QComboBox::down-arrow {image:none;width:0;height:0;}
            #connectionHome QPushButton::menu-indicator {image:none;width:0;}
            #connectionHome QPushButton:hover,#connectionHome QComboBox:hover {background:%6;}
            #connectionHome QPushButton:focus,#connectionHome QComboBox:focus {border:1px solid %3;}
            #connectionHome QPushButton:disabled {color:%3;}
            #connectionHome QPushButton#power {background:#d2f793;color:#1c2719;font-weight:650;border-radius:11px;padding:8px 15px;min-height:21px;}
            #connectionHome QPushButton#power:hover {background:#e1ffb0;}
            #connectionHome QPushButton#power:focus {border:1px solid white;}
            #connectionHome QPushButton#power:disabled {background:#414d42;color:#c2cec2;}
            #connectionHome QListWidget,#connectionHome QListWidget > QWidget {background:transparent;border:0;outline:none;padding:0;margin:0;}
            #connectionHome QMenu,#connectionHome QComboBox QAbstractItemView {background:%1;color:%2;border:1px solid %4;selection-background-color:%6;selection-color:%2;}
            #connectionHome QMenu::item:selected {background:%6;color:%2;}
        )").arg(bg,ink,muted,line,well,highlight));
        using namespace ConnectionVisuals;
        power->setIcon(glyphIcon(Glyph::Power,QColor("#1c2719")));
        preferences->setIcon(glyphIcon(Glyph::Settings,QColor(ink)));
        add->setIcon(glyphIcon(Glyph::Plus,QColor(ink)));
        refresh->setIcon(glyphIcon(Glyph::Refresh,QColor(muted)));
        servers->viewport()->update();
    }

    void changeEvent(QEvent *event) override {
        QWidget::changeEvent(event);
        if (event->type() == QEvent::ApplicationPaletteChange) paintHome();
    }

    int selectedId() const {
        return servers->currentItem() ? servers->currentItem()->data(Qt::UserRole).toInt() : -1;
    }

    void reloadGroups(int preferredGroup = -1) {
        int gid = preferredGroup >= 0 ? preferredGroup : groups->currentIndex() >= 0
            ? groups->currentData().toInt() : settings->value("group",Configs::dataManager->settingsRepo->current_group).toInt();
        QSignalBlocker blocker(groups);
        const int selected = selectedId();
        const bool initialGroup = preferredGroup < 0 && groups->currentIndex() < 0 && !settings->contains("group");
        groups->clear();
        for (int id : Configs::dataManager->groupsRepo->GetGroupsTabOrder()) {
            const auto group = Configs::dataManager->groupsRepo->GetGroup(id);
            if (group && !group->archive) groups->addItem(group->name,id);
        }
        const auto current = Configs::dataManager->groupsRepo->GetGroup(gid);
        if (initialGroup && (!current || current->Profiles().isEmpty())) {
            for (int index = 0; index < groups->count(); ++index) {
                const auto candidate = Configs::dataManager->groupsRepo->GetGroup(groups->itemData(index).toInt());
                if (candidate && !candidate->Profiles().isEmpty()) {
                    gid = candidate->id;
                    break;
                }
            }
        }
        groups->setCurrentIndex(qMax(groups->findData(gid), groups->count() ? 0 : -1));
        groups->setMinimumContentsLength(qBound(12,groups->currentText().size(),32));
        reloadProfiles(selected);
    }

    void reloadProfiles(int preferred = -1) {
        QSignalBlocker blocker(servers);
        servers->clear();
        const int gid = groups->currentIndex() < 0 ? -1 : groups->currentData().toInt();
        const auto group = Configs::dataManager->groupsRepo->GetGroup(gid);
        if (preferred < 0) preferred = settings->value("server/" + QString::number(gid), -1).toInt();
        int target = -1, active = -1;
        if (group) {
            const auto entries = Configs::dataManager->profilesRepo->GetProfileIDNameMappedBatch(group->Profiles());
            for (const auto &[id,name] : entries) {
                auto *row = new QListWidgetItem(name,servers);
                row->setData(Qt::UserRole,id);
                if (id == preferred) target = servers->count()-1;
                if (id == Configs::dataManager->settingsRepo->started_id) active = servers->count()-1;
            }
        }
        servers->setCurrentRow(target >= 0 ? target : active >= 0 ? active : 0);
        servers->setVerticalScrollBarPolicy(servers->count() > 7 ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
        servers->setFixedHeight(qMin(servers->count(), 7)*52);
        servers->setVisible(servers->count() > 0);
        empty->setText(group && !group->url.isEmpty() ? "В подписке пока нет серверов." : "Добавь подписку, чтобы выбрать сервер.");
        empty->setVisible(servers->count() == 0);
        refresh->setEnabled(group && !group->url.isEmpty());
    }

    void updateStatus(Engine::ConnectionManager *connection) {
        const auto phase = connection->connectionState();
        const bool running = phase == Engine::ConnectionState::Running;
        const bool busy = phase == Engine::ConnectionState::Connecting || phase == Engine::ConnectionState::Stopping;
        const auto profile = connection->runningSnapshot();
        const int selected = selectedId();
        const auto &cfg = Configs::dataManager->settingsRepo;
        state->setText(busy ? (phase == Engine::ConnectionState::Connecting ? "Подключение…" : "Отключение…")
                            : running ? "Подключено" : "Отключено");
        QStringList modes;
        if (cfg->spmode_vpn) modes << "TUN";
        if (cfg->spmode_system_proxy) modes << "Системный прокси";
        if (modes.isEmpty()) modes << "Локальный прокси";
        mode->setText("Соединение   ·   " + modes.join(" + "));
        QString location;
        if (running && profile) location = profile->name;
        else if (servers->currentItem()) location = servers->currentItem()->text();
        detail->setToolTip(location);
        const int labelWidth = qMax(160,int(banner->width()*0.62));
        detail->setText(detail->fontMetrics().elidedText(location,Qt::ElideRight,labelWidth));
        banner->setConnected(running);
        const bool switchServer = running && profile && selected >= 0 && profile->id != selected;
        power->setText(switchServer ? "Сменить сервер" : running ? "Отключить" : "Подключить");
        power->setEnabled(!busy && (running || selected >= 0));
        stats->setVisible(running && bool(profile));
        if (profile) {
            const QLocale locale(QLocale::Russian);
            const auto size = [&locale](qint64 bytes) {
                return bytes < 1024 ? locale.toString(bytes) + " Б" : locale.formattedDataSize(bytes,1);
            };
            down->setText(size(profile->traffic_downlink));
            up->setText(size(profile->traffic_uplink));
        }
    }

    ConnectionVisuals::Banner *banner;
    QWidget *stats;
    QLabel *mode,*down,*up;
    QComboBox *groups;
    QListWidget *servers;
    QPushButton *preferences, *advanced, *power, *add, *refresh;
    QLabel *state, *detail, *empty;
    QSettings *settings;

private:
    QLabel *text(const QString &value, const QString &name) {
        auto *label = new QLabel(value,this);
        label->setTextFormat(Qt::PlainText);
        label->setObjectName(name);
        return label;
    }

    QPushButton *button(const QString &name, QHBoxLayout *layout) {
        auto *result = new QPushButton(name,this);
        result->setCursor(Qt::PointingHandCursor);
        layout->addWidget(result);
        return result;
    }
};

} // namespace

void MainWindow::setupConnectionHome() {
    setWindowIcon(QIcon(":/Throne/Throne.png"));
    auto *classic = takeCentralWidget();
    auto *stack = new QStackedWidget(this);
    stack->setObjectName("clientViews");
    stack->setStyleSheet("QStackedWidget#clientViews {border:none;padding:0;margin:0;}");
    auto *home = new ConnectionHome(stack);
    stack->addWidget(classic);
    stack->addWidget(home);
    setCentralWidget(stack);

    const auto update = [this,home] { home->updateStatus(connection()); };
    auto *toHome = new QAction(QStringLiteral("Основной вид"),this);
    ui->menu_program->insertAction(ui->menu_program->actions().value(0),toHome);
    auto *returnButton = new QPushButton(toHome->text(),ui->tabWidget);
    auto *corner = qobject_cast<QHBoxLayout*>(ui->tabWidget->cornerWidget()->layout());
    if (corner) corner->insertWidget(0,returnButton);
    connect(returnButton,&QPushButton::clicked,toHome,&QAction::trigger);
    const auto showPage = [this,stack,home,classic,update](bool advanced) {
        primaryViewVisible = !advanced;
        stack->setCurrentWidget(advanced ? classic : static_cast<QWidget*>(home));
        home->settings->setValue("view", advanced ? "advanced" : "home");
        home->settings->sync();
        refresh_status();
        if (!advanced) {
            home->reloadGroups();
            update();
        }
    };
    connect(toHome,&QAction::triggered,this,[showPage]{showPage(false);});
    connect(home->advanced,&QPushButton::clicked,this,[showPage]{showPage(true);});
    connect(home->add,&QPushButton::clicked,this,[this]{handle_addsub({},{});});
    auto *settingsMenu = new QMenu(home);
    settingsMenu->addAction(ui->menu_spmode_vpn);
    settingsMenu->addAction(ui->menu_spmode_system_proxy);
    connect(settingsMenu,&QMenu::aboutToShow,home,[this] {
        const auto &settings = Configs::dataManager->settingsRepo;
        ui->menu_spmode_vpn->setChecked(settings->spmode_vpn);
        ui->menu_spmode_system_proxy->setChecked(settings->spmode_system_proxy);
    });
    settingsMenu->addSeparator();
    settingsMenu->addAction(ui->menu_vpn_settings);
    settingsMenu->addAction(ui->menu_routing_settings);
    settingsMenu->addAction(ui->menu_manage_groups);
    settingsMenu->addAction(ui->menu_basic_settings);
    home->preferences->setMenu(settingsMenu);
    connect(home->refresh,&QPushButton::clicked,this,[home] {
        if (home->groups->currentIndex() >= 0)
            Subscription::updater()->RefreshGroup(home->groups->currentData().toInt(),nullptr,Subscription::GroupUpdater::Surface::Report);
    });
    connect(home->groups,&QComboBox::currentIndexChanged,this,[home,update](int index) {
        if (index >= 0) {
            home->settings->setValue("group",home->groups->currentData());
            home->groups->setMinimumContentsLength(qBound(12,home->groups->currentText().size(),32));
        }
        home->reloadProfiles();
        update();
    });
    connect(home->servers,&QListWidget::currentRowChanged,this,[home,update](int row) {
        if (row >= 0) home->settings->setValue("server/" + QString::number(home->groups->currentData().toInt()),home->selectedId());
        update();
    });
    connect(home->power,&QPushButton::clicked,this,[this,home] {
        const auto current = runningSnapshot();
        if (connection()->connectionState() == Engine::ConnectionState::Running &&
            (home->selectedId() < 0 || (current && current->id == home->selectedId())))
            profile_stop(false,false,true);
        else if (home->selectedId() >= 0)
            profile_start(home->selectedId());
    });
    auto *events = Engine::EngineEvents::instance();
    const auto reload = [home,stack,update] {
        if (stack->currentWidget() == home) {
            home->reloadGroups();
            update();
        }
    };
    connect(events,&Engine::EngineEvents::groupsChanged,home,reload);
    connect(events,&Engine::EngineEvents::profilesChanged,home,reload);
    connect(events,&Engine::EngineEvents::profilesRemoved,home,reload);
    connect(events,&Engine::EngineEvents::subscriptionGroupChanged,home,reload);
    connect(events,&Engine::EngineEvents::subscriptionFinished,home,reload);
    connect(events,&Engine::EngineEvents::subscriptionNewGroup,home,[home,update](int id){
        home->reloadGroups(id);
        update();
    });
    connect(connection(),&Engine::ConnectionManager::connectionStateChanged,home,update);
    connect(connection(),&Engine::ConnectionManager::statusChanged,home,update);
    auto *timer = new QTimer(home);
    connect(timer,&QTimer::timeout,home,[home,update]{if (home->isVisible()) update();});
    timer->start(2000);
    showPage(home->settings->value("view","home").toString() == "advanced");
}
