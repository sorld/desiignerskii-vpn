#include "include/ui/mainwindow.h"
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
        auto *outer = new QVBoxLayout(this);
        outer->setContentsMargins(28, 20, 28, 20);
        auto *header = new QHBoxLayout;
        auto *mark = new QLabel(this);
        mark->setPixmap(QIcon(":/Throne/Throne.png").pixmap(28,28));
        header->addWidget(mark);
        header->addWidget(text("Desiignerskii VPN", "wordmark"));
        header->addStretch();
        preferences = button("Настройки", header);
        advanced = button("Расширенный вид", header);
        outer->addLayout(header);
        auto *center = new QHBoxLayout;
        auto *content = new QWidget(this);
        content->setMaximumWidth(920);
        center->addStretch();
        center->addWidget(content, 9);
        center->addStretch();
        outer->addLayout(center, 1);

        auto *body = new QVBoxLayout(content);
        body->setContentsMargins(0, 24, 0, 0);
        body->setSpacing(12);
        auto *summary = new QHBoxLayout;
        auto *copy = new QVBoxLayout;
        state = text({}, "state");
        detail = text({}, "muted");
        copy->addWidget(state);
        copy->addWidget(detail);
        summary->addLayout(copy,1);
        power = button("Подключить", summary);
        power->setObjectName("power");
        body->addLayout(summary);
        body->addSpacing(12);
        auto *titleRow = new QHBoxLayout;
        titleRow->addWidget(text("Серверы", "heading"));
        titleRow->addStretch();
        add = button("Добавить подписку", titleRow);
        body->addLayout(titleRow);
        auto *filterRow = new QHBoxLayout;
        groups = new QComboBox(this);
        groups->setAccessibleName("Подписка");
        groups->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        filterRow->addWidget(groups,1);
        refresh = button("Обновить",filterRow);
        body->addLayout(filterRow);
        servers = new QListWidget(this);
        servers->setAccessibleName("Серверы");
        servers->setUniformItemSizes(true);
        servers->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        empty = text("Добавь ссылку подписки, чтобы выбрать сервер.", "muted");
        empty->setAlignment(Qt::AlignCenter);
        body->addWidget(empty,1);
        body->addWidget(servers,1);
        traffic = text({}, "muted");
        body->addWidget(traffic);
        settings = new QSettings(QDir::current().absoluteFilePath("desiignerskii-ui.ini"), QSettings::IniFormat, this);
        paintHome();
    }

    void paintHome() {
        const bool dark = qApp->palette().color(QPalette::Window).lightness() < 128;
        const QString bg = dark ? "#191c1b" : "#ffffff";
        const QString ink = dark ? "#f2f4f2" : "#181a19";
        const QString muted = dark ? "#b3b9b4" : "#575e5b";
        const QString well = dark ? "#282c29" : "#f2f4f0";
        const QString edge = dark ? "#3a403b" : "#e2e5e2";
        const QString selected = dark ? "#364235" : "#e6f2cf";
        setStyleSheet(QStringLiteral(
            "QWidget#connectionHome{background:%1;} #connectionHome QLabel{background:transparent;color:%2;font:14px 'Segoe UI';border:0;}"
            "#connectionHome QLabel#wordmark{font-size:18px;font-weight:700;} #connectionHome QLabel#state{font-size:32px;font-weight:650;}"
            "#connectionHome QLabel#heading{font-size:21px;font-weight:650;} #connectionHome QLabel#muted{color:%3;}"
            "#connectionHome QPushButton,#connectionHome QComboBox{background:%4;color:%2;border:1px solid %5;"
            "border-radius:9px;padding:8px 13px;font:14px 'Segoe UI';}"
            "#connectionHome QPushButton:hover{background:%6;} #connectionHome QPushButton:focus,#connectionHome QListWidget:focus,"
            "#connectionHome QComboBox:focus{border:1px solid #79a348;} #connectionHome QPushButton:disabled{color:%3;}"
            "#connectionHome QPushButton#power{background:#c1ef71;color:#192416;border:0;padding:12px 24px;font-weight:650;}"
            "#connectionHome QPushButton#power:disabled{background:%4;color:%3;}"
            "#connectionHome QListWidget{background:%1;color:%2;border:1px solid %5;border-radius:10px;padding:4px;outline:0;font:15px 'Segoe UI';}"
            "#connectionHome QListWidget::item{padding:9px 12px;border-radius:6px;} #connectionHome QListWidget::item:selected{background:%6;color:%2;}"
            "#connectionHome QComboBox QAbstractItemView{background:%1;color:%2;selection-background-color:%6;selection-color:%2;}"
        ).arg(bg,ink,muted,well,edge,selected));
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
        groups->clear();
        for (int id : Configs::dataManager->groupsRepo->GetGroupsTabOrder()) {
            const auto group = Configs::dataManager->groupsRepo->GetGroup(id);
            if (group && !group->archive) groups->addItem(group->name,id);
        }
        groups->setCurrentIndex(qMax(groups->findData(gid), groups->count() ? 0 : -1));
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
        servers->setVisible(servers->count() > 0);
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
        QString caption = modes.join(" + ");
        if (running && profile) caption += "  ·  " + profile->name;
        detail->setText(caption);
        const bool switchServer = running && profile && selected >= 0 && profile->id != selected;
        power->setText(switchServer ? "Сменить сервер" : running ? "Отключить" : "Подключить");
        power->setEnabled(!busy && (running || selected >= 0));
        traffic->setVisible(running && bool(profile));
        if (profile) {
            const auto locale = QLocale();
            traffic->setText("Принято  " + locale.formattedDataSize(profile->traffic_downlink) +
                             "     Отправлено  " + locale.formattedDataSize(profile->traffic_uplink));
        }
    }

    QComboBox *groups;
    QListWidget *servers;
    QPushButton *preferences, *advanced, *power, *add, *refresh;
    QLabel *state, *detail, *empty, *traffic;
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
        layout->addWidget(result);
        return result;
    }
};

} // namespace

void MainWindow::setupConnectionHome() {
    setWindowIcon(QIcon(":/Throne/Throne.png"));
    auto *classic = takeCentralWidget();
    auto *stack = new QStackedWidget(this);
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
    const auto showPage = [stack,home,classic,update](bool advanced) {
        stack->setCurrentWidget(advanced ? classic : static_cast<QWidget*>(home));
        home->settings->setValue("view", advanced ? "advanced" : "home");
        home->settings->sync();
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
        if (index >= 0) home->settings->setValue("group",home->groups->currentData());
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
