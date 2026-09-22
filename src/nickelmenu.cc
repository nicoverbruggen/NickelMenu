#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QFile>
#include <QLayout>
#include <QMenu>
#include <QMetaProperty>
#include <QPushButton>
#include <QRegularExpression>
#include <QString>
#include <QUrl>
#include <QWidget>
#include <QWidgetAction>

#include <cstdlib>

#include <NickelHook.h>
#include "native.h"

#include "action.h"
#include "config.h"
#include "nickelmenu.h"
#include "util.h"

typedef QWidget MenuTextItem; // it's actually a subclass, but we don't need its functionality directly, so we'll stay on the safe side
typedef void MainWindowController;

// AbstractNickelMenuController::createMenuTextItem creates a menu item
// (allocated on the heap). The last parameter has been empty everywhere I've
// seen it. The checked option does not have any effect unless checkable is true
// (see the search menu for an example).
static MenuTextItem* (*AbstractNickelMenuController_createMenuTextItem)(void*, QMenu* menu, QString const& text, bool checkable, bool checked, QString const& /*I think it's for QObject::setObjectName*/);

// AbstractNickelMenuController::createAction finishes adding the action to the
// menu. The actionWidget should be a MenuTextItem (allocated on the heap). If
// disabled is true, the item will appear greyed out. Note that for the
// separator, even in the main menu (removed in FW 15505), it'll inherit the
// color of a Reader Menu separator (#666), rather than the usual dim (#BBB) or
// solid (#000) ones seen in the stock main menu.
//
// The code is basically:
//     act = new QWidgetAction(this, arg2);
//     act->setEnabled(arg4);
//     if (arg3)
//         connect(act, &QAction::triggered, &QWidget::hide, arg1);
//     arg1->addAction(act);
//     if (arg5)
//         arg1->addSeparator();
static QAction* (*AbstractNickelMenuController_createAction)(void*, QMenu* menu, QWidget* actionWidget, bool closeOnTap, bool disabled, bool separatorAfter);

// ConfirmationDialogFactory::showOKDialog shows an dialog box with an OK
// button, and should only be called from the main thread (or a signal handler).
// Note that ConfirmationDialog uses Qt::RichText for the body.
static void (*ConfirmationDialogFactory_showOKDialog)(QString const& title, QString const& body);

MainWindowController *(*MainWindowController_sharedInstance)();

// MainWindowController::toast shows a message as an overlay, and should only be
// called from the main thread (or a signal handler).
void (*MainWindowController_toast)(MainWindowController*, QString const& primary, QString const& secondary, int milliseconds);

// *MenuSeparator::*MenuSeparator initializes a main menu separator which can be
// added to the menu with QWidget::addAction. The menu should be its parent. It
// initializes a QAction.
static void (*LightMenuSeparator_LightMenuSeparator)(void*, QWidget* parent);
static void (*BoldMenuSeparator_BoldMenuSeparator)(void*, QWidget* parent);

// New bottom tab bar which replaced the main menu on 15505+.
typedef QWidget MainNavButton;
typedef QWidget MainNavView;
void (*MainNavView_MainNavView)(MainNavView*, QWidget* parent);
void (*MainNavButton_MainNavButton)(MainNavButton*, QWidget* parent);
void (*MainNavButton_setPixmap)(MainNavButton*, QString const& pixmapName);
void (*MainNavButton_setActivePixmap)(MainNavButton*, QString const& pixmapName);
void (*MainNavButton_setText)(MainNavButton*, QString const& text);
void (*MainNavButton_tapped)(MainNavButton*); // signal

// Creating menus from scratch (for the menu above on 15505+).
typedef QMenu TouchMenu;
typedef TouchMenu NickelTouchMenu;
typedef int DecorationPosition;
void (*NickelTouchMenu_NickelTouchMenu)(NickelTouchMenu*, QWidget* parent, DecorationPosition position);
void (*MenuTextItem_MenuTextItem)(MenuTextItem*, QWidget* parent, bool checkable, bool italic);
void (*MenuTextItem_setText)(MenuTextItem*, QString const& text);
void (*MenuTextItem_registerForTapGestures)(MenuTextItem*);

// Selection menu stuff (14622+).
typedef void SelectionMenuController; // note: items are re-initialized every time the menu is opened
typedef QWidget SelectionMenuView;
typedef void WebSearchMixinBase;
void (*SelectionMenuController_lookupWikipedia)(SelectionMenuController*);
void (*SelectionMenuController_setupSearchOptions)(SelectionMenuController*, bool);
SelectionMenuView *(*SelectionMenuController_menuView)(SelectionMenuController*);
void (*SelectionMenuController_close)(SelectionMenuController*);
void (*SelectionMenuController_clearSelection)(SelectionMenuController*);
QString (*PageImageReader_selectedText)(QWidget*);
void (*SelectionMenuController_onSearchInBook) (SelectionMenuController*);
void (*SelectionMenuController_lookupWeb)(SelectionMenuController*); // 14622-18838
void (*SelectionMenuController_lookupGoogle)(SelectionMenuController*); // 19086+ (replaces lookupWeb, alternative 1)
void (*SelectionMenuController_lookupBaidu)(SelectionMenuController*); // 19086+ (replaces lookupWeb, alternative 2)
void (*SelectionMenuController_addMenuItem)(SelectionMenuController*, SelectionMenuView* smv, MenuTextItem* mti, const char *slot); // note: the MenuTextItem is created by SelectionMenuController_createMenuTextItem, with smv as the parent (the first QWidget* argument)
void (*SelectionMenuView_addMenuItem)(SelectionMenuView*, MenuTextItem *mti); // note: this adds the separator and the item (it doesn't connect signals or things like that)
void (*WebSearchMixinBase_doWikipediaSearch)(WebSearchMixinBase *, QString const& selection, QString const& locale);

static struct nh_info NickelMenu = (struct nh_info){
    .name            = "NickelMenu",
    .desc            = "Integrated launcher for Nickel.",
    .uninstall_flag  = NM_CONFIG_DIR "/uninstall",
#ifdef NM_UNINSTALL_CONFIGDIR
    .uninstall_xflag = NM_CONFIG_DIR,
#else
    .uninstall_xflag = NULL,
#endif
    .failsafe_delay  = 3,
};

static struct nh_hook NickelMenuHook[] = {
    {.sym = "_ZN23SelectionMenuController18setupSearchOptionsEb", .sym_new = "_nm_selection_search", .lib = "libnickel.so.1.0.0", .out = nh_symoutptr(SelectionMenuController_setupSearchOptions), .desc = nullptr, .optional = true}, //libnickel 6.0 * _ZN23SelectionMenuController18setupSearchOptionsEb
    // menu injection
    {.sym = "_ZN28AbstractNickelMenuController18createMenuTextItemEP5QMenuRK7QStringbbS4_", .sym_new = "_nm_menu_hook", .lib = "libnickel.so.1.0.0", .out = nh_symoutptr(AbstractNickelMenuController_createMenuTextItem), .desc = nullptr, .optional = true}, //libnickel 4.6 * _ZN28AbstractNickelMenuController18createMenuTextItemEP5QMenuRK7QStringbbS4_

    // bottom nav main menu button injection (15505+)
    {.sym = "_ZN11MainNavViewC1EP7QWidget", .sym_new = "_nm_menu_hook2", .lib = "libnickel.so.1.0.0", .out = nh_symoutptr(MainNavView_MainNavView), .desc = "bottom nav main menu button injection (15505+)", .optional = true}, //libnickel 4.23.15505 * _ZN11MainNavViewC1EP7QWidget

    // selection menu injection
    {.sym = "_ZN23SelectionMenuController11addMenuItemEP17SelectionMenuViewP12MenuTextItemPKc", .sym_new = "_nm_menu_hook3", .lib = "libnickel.so.1.0.0", .out = nh_symoutptr(SelectionMenuController_addMenuItem),  .desc = "selection menu injection",                     .optional = true}, //libnickel 4.20.14622 * _ZN23SelectionMenuController11addMenuItemEP17SelectionMenuViewP12MenuTextItemPKc
    {.sym = "_ZN18WebSearchMixinBase17doWikipediaSearchERK7QStringS2_",                         .sym_new = "_nm_menu_hook4", .lib = "libnickel.so.1.0.0", .out = nh_symoutptr(WebSearchMixinBase_doWikipediaSearch), .desc = "selection menu injection (wikipedia handler)", .optional = true}, //libnickel 4.20.14622 * _ZN18WebSearchMixinBase17doWikipediaSearchERK7QStringS2_

    // null
    {0},
};

static struct nh_dlsym NickelMenuDlsym[] = {
    {.name = "_ZN23SelectionMenuController14clearSelectionEv", .out = nh_symoutptr(SelectionMenuController_clearSelection), .desc = nullptr, .optional = true}, //libnickel 6.0 * _ZN23SelectionMenuController14clearSelectionEv
    {.name = "_ZN23SelectionMenuController5closeEv", .out = nh_symoutptr(SelectionMenuController_close), .desc = nullptr, .optional = true}, //libnickel 6.0 * _ZN23SelectionMenuController5closeEv
    {.name = "_ZN15PageImageReader12selectedTextEv", .out = nh_symoutptr(PageImageReader_selectedText), .desc = nullptr, .optional = true}, //libnickel 6.0 * _ZN15PageImageReader12selectedTextEv
    {.name = "_ZN23SelectionMenuController8menuViewEv", .out = nh_symoutptr(SelectionMenuController_menuView), .desc = nullptr, .optional = true}, //libnickel 6.0 * _ZN23SelectionMenuController8menuViewEv
    // menu injection
    {.name = "_ZN28AbstractNickelMenuController18createMenuTextItemEP5QMenuRK7QStringbbS4_", .out = nh_symoutptr(AbstractNickelMenuController_createMenuTextItem), .desc = nullptr, .optional = true}, //libnickel 4.6 * _ZN28AbstractNickelMenuController18createMenuTextItemEP5QMenuRK7QStringbbS4_
    {.name = "_ZN22AbstractMenuController12createActionEP5QMenuP7QWidgetbbb",                .out = nh_symoutptr(AbstractNickelMenuController_createAction), .desc = nullptr, .optional = true},       //libnickel 4.6 * _ZN22AbstractMenuController12createActionEP5QMenuP7QWidgetbbb
    {.name = "_ZN25ConfirmationDialogFactory12showOKDialogERK7QStringS2_",                   .out = nh_symoutptr(ConfirmationDialogFactory_showOKDialog), .desc = nullptr, .optional = true},          //libnickel 4.6 * _ZN25ConfirmationDialogFactory12showOKDialogERK7QStringS2_
    {.name = "_ZN20MainWindowController14sharedInstanceEv",                                  .out = nh_symoutptr(MainWindowController_sharedInstance), .desc = nullptr, .optional = true},             //libnickel 4.6 * _ZN20MainWindowController14sharedInstanceEv
    {.name = "_ZN20MainWindowController5toastERK7QStringS2_i",                               .out = nh_symoutptr(MainWindowController_toast), .desc = nullptr, .optional = true},                      //libnickel 4.6 * _ZN20MainWindowController5toastERK7QStringS2_i
    {.name = "_ZN18LightMenuSeparatorC2EP7QWidget",                                          .out = nh_symoutptr(LightMenuSeparator_LightMenuSeparator), .desc = nullptr, .optional = true},           //libnickel 4.6 * _ZN18LightMenuSeparatorC2EP7QWidget
    {.name = "_ZN17BoldMenuSeparatorC1EP7QWidget",                                           .out = nh_symoutptr(BoldMenuSeparator_BoldMenuSeparator), .desc = nullptr, .optional = true},             //libnickel 4.6 * _ZN17BoldMenuSeparatorC1EP7QWidget

    // bottom nav main menu button injection (15505+)
    {.name = "_ZN13MainNavButtonC1EP7QWidget",                       .out = nh_symoutptr(MainNavButton_MainNavButton),         .desc = "bottom nav main menu button injection (15505+)", .optional = true}, //libnickel 4.23.15505 * _ZN13MainNavButtonC1EP7QWidget
    {.name = "_ZN13MainNavButton9setPixmapERK7QString",              .out = nh_symoutptr(MainNavButton_setPixmap),             .desc = "bottom nav main menu button injection (15505+)", .optional = true}, //libnickel 4.23.15505 * _ZN13MainNavButton9setPixmapERK7QString
    {.name = "_ZN13MainNavButton15setActivePixmapERK7QString",       .out = nh_symoutptr(MainNavButton_setActivePixmap),       .desc = "bottom nav main menu button injection (15505+)", .optional = true}, //libnickel 4.23.15505 * _ZN13MainNavButton15setActivePixmapERK7QString
    {.name = "_ZN13MainNavButton7setTextERK7QString",                .out = nh_symoutptr(MainNavButton_setText),               .desc = "bottom nav main menu button injection (15505+)", .optional = true}, //libnickel 4.23.15505 * _ZN13MainNavButton7setTextERK7QString
    {.name = "_ZN13MainNavButton6tappedEv",                          .out = nh_symoutptr(MainNavButton_tapped),                .desc = "bottom nav main menu button injection (15505+)", .optional = true}, //libnickel 4.23.15505 * _ZN13MainNavButton6tappedEv
    {.name = "_ZN15NickelTouchMenuC2EP7QWidget18DecorationPosition", .out = nh_symoutptr(NickelTouchMenu_NickelTouchMenu),     .desc = "bottom nav main menu button injection (15505+)", .optional = true}, //libnickel 4.23.15505 * _ZN15NickelTouchMenuC2EP7QWidget18DecorationPosition
    {.name = "_ZN12MenuTextItemC1EP7QWidgetbb",                      .out = nh_symoutptr(MenuTextItem_MenuTextItem),           .desc = "bottom nav main menu button injection (15505+)", .optional = true}, //libnickel 4.23.15505 * _ZN12MenuTextItemC1EP7QWidgetbb
    {.name = "_ZN12MenuTextItem7setTextERK7QString",                 .out = nh_symoutptr(MenuTextItem_setText),                .desc = "bottom nav main menu button injection (15505+)", .optional = true}, //libnickel 4.23.15505 * _ZN12MenuTextItem7setTextERK7QString
    {.name = "_ZN12MenuTextItem22registerForTapGesturesEv",          .out = nh_symoutptr(MenuTextItem_registerForTapGestures), .desc = "bottom nav main menu button injection (15505+)", .optional = true}, //libnickel 4.23.15505 * _ZN12MenuTextItem22registerForTapGesturesEv

    // selection menu injection (14622+)
    {.name = "_ZN17SelectionMenuView11addMenuItemEP12MenuTextItem", .out = nh_symoutptr(SelectionMenuView_addMenuItem),           .desc = "selection menu injection (14622+)",        .optional = true}, //libnickel 4.20.14622 * _ZN17SelectionMenuView11addMenuItemEP12MenuTextItem
    {.name = "_ZN23SelectionMenuController15lookupWikipediaEv",     .out = nh_symoutptr(SelectionMenuController_lookupWikipedia), .desc = "selection menu injection (14622+)",        .optional = true}, //libnickel 4.20.14622 * _ZN23SelectionMenuController15lookupWikipediaEv _ZN23SelectionMenuController15lookupWikipediaERK7QString
    {.name = "_ZN23SelectionMenuController9lookupWebEv",            .out = nh_symoutptr(SelectionMenuController_lookupWeb),       .desc = "selection menu injection (14622-18838)",   .optional = true}, //libnickel 4.20.14622 4.30.18838 _ZN23SelectionMenuController9lookupWebEv
    {.name = "_ZN23SelectionMenuController12lookupGoogleEv",        .out = nh_symoutptr(SelectionMenuController_lookupGoogle),    .desc = "selection menu injection (19086+, alt 1)", .optional = true}, //libnickel 4.31.19086 * _ZN23SelectionMenuController12lookupGoogleEv _ZN23SelectionMenuController12lookupGoogleERK7QString _ZN23SelectionMenuController9lookupWebEv
    {.name = "_ZN23SelectionMenuController11lookupBaiduEv",         .out = nh_symoutptr(SelectionMenuController_lookupBaidu),     .desc = "selection menu injection (19086+, alt 2)", .optional = true}, //libnickel 4.31.19086 * _ZN23SelectionMenuController11lookupBaiduEv _ZN23SelectionMenuController11lookupBaiduERK7QString

    // selection menu injection (parental controls enabled)
    {.name = "_ZN23SelectionMenuController14onSearchInBookEv",     .out = nh_symoutptr(SelectionMenuController_onSearchInBook), .desc = "selection menu injection (14601+, parental controls enabled)", .optional = true}, //libnickel 4.20.14601 * _ZN23SelectionMenuController14onSearchInBookEv

    // null
    {0},
};

static int nm_init();

NickelHook(
    .init  = &nm_init,
    .info  = &NickelMenu,
    .hook  = NickelMenuHook,
    .dlsym = NickelMenuDlsym,
)

// AbstractNickelMenuController_createAction_before wraps
// AbstractNickelMenuController::createAction to use the correct separator for
// the menu location and to match the behaviour of QMenu::insertAction instead
// of QMenu::addAction. It also adds the property nm_action=true to the action
// and separator.
QAction *AbstractNickelMenuController_createAction_before(QAction *before, nm_menu_location_t loc, bool last_in_group, void *_this, QMenu *menu, QWidget *widget, bool close, bool enabled, bool separator);

// nm_argtranform_t transforms an action's argument and returns a new malloc'd
// string. On error, it should return NULL and set nm_err.
typedef char *(*nm_argtransform_t)(void *data, const char *arg);

// nm_menu_item_do runs a nm_menu_item_t and must be called from the thread of a
// signal handler. argtransform and argtransform_data are optional.
static void nm_menu_item_do(nm_menu_item_t *it, nm_argtransform_t argtransform, void *argtransform_data);

// _nm_menu_inject handles the QMenu::aboutToShow signal and injects menu items.
static void _nm_menu_inject(void *nmc, QMenu *menu, nm_menu_location_t loc, int at);

static void nm_show_dialog(QString const &title, QString const &body) {
    if (ConfirmationDialogFactory_showOKDialog)
        ConfirmationDialogFactory_showOKDialog(title, body);
    else
        NM_LOG("dialog unavailable: %s: %s", qPrintable(title), qPrintable(body));
}

static int nm_init() {
    // Each menu and output feature checks its own dependencies before use.
    for (auto *symbol = NickelMenuDlsym; symbol->name; ++symbol) {
        auto *code = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(*symbol->out) & ~uintptr_t(1));
        if (*symbol->out && !nh_native_range(code, 2, PF_R | PF_X)) {
            NM_LOG("symbol outside executable memory: %s", symbol->name);
            *symbol->out = nullptr;
        }
    }
    #ifdef NM_UNINSTALL_CONFIGDIR
    NM_LOG("feature: NM_UNINSTALL_CONFIGDIR: true");
    #else
    NM_LOG("feature: NM_UNINSTALL_CONFIGDIR: false");
    #endif

    NM_LOG("updating config");

    int rev = nm_global_config_update();
    if (nm_err_peek())
        NM_LOG("... warning: error parsing config, will show a menu item with the error: %s", nm_err());

    size_t ntmp = SIZE_MAX;
    if (rev == -1) {
        NM_LOG("... info: no config file changes detected for initial config update (it should always return an error or update), stopping (this is a bug; err should have been returned instead)");
    } else if (!nm_global_config_items(&ntmp)) {
        NM_LOG("... warning: no menu items returned by nm_global_config_items, ignoring for now (this is a bug; it should always have a menu item whether the default, an error, or the actual config)");
    } else if (ntmp == SIZE_MAX) {
        NM_LOG("... warning: no size returned by nm_global_config_items, ignoring for now (this is a bug)");
    } else if (!ntmp) {
        NM_LOG("... warning: size returned by nm_global_config_items is 0, ignoring for now (this is a bug; it should always have a menu item whether the default, an error, or the actual config)");
    }

    return 0;
}

extern "C" __attribute__((visibility("default"))) MenuTextItem* _nm_menu_hook(void* _this, QMenu* menu, QString const& label, bool checkable, bool checked, QString const& thingy) {
    NM_LOG("AbstractNickelMenuController::createMenuTextItem(%p, `%s`, %d, %d, `%s`)", menu, qPrintable(label), checkable, checked, qPrintable(thingy));

    QString trmm = QCoreApplication::translate("StatusBarMenuController", "Settings");
    QString trrm = QCoreApplication::translate("DictionaryActionProxy", "Dictionary");
    QString trbm = QCoreApplication::translate("N3BrowserSettingsMenuController", "Keyboard");
    QString trlm = QCoreApplication::translate("LibraryViewMenuController", "Manage downloads");
    NM_LOG("Comparing against '%s', '%s', '%s', '%s'", qPrintable(trmm), qPrintable(trrm), qPrintable(trbm), qPrintable(trlm));

    nm_menu_location_t loc = {};
    if (label == trmm && !checkable) {
        NM_LOG("Intercepting main menu (label=Settings, checkable=false)...");
        loc = NM_MENU_LOCATION(main);
    } else if (label == trrm && !checkable) {
        NM_LOG("Intercepting reader menu (label=Dictionary, checkable=false)...");
        loc = NM_MENU_LOCATION(reader);
    } else if (label == trbm && !checkable) {
        NM_LOG("Intercepting browser menu (label=Keyboard, checkable=false)...");
        loc = NM_MENU_LOCATION(browser);
    } else if (label == trlm && !checkable) {
        NM_LOG("Intercepting library menu (label=Manage downloads, checkable=false)...");  // this is actually two menus: in "My Books", and in "My Articles"
        loc = NM_MENU_LOCATION(library);
    }

    if (loc && AbstractNickelMenuController_createAction)
        QObject::connect(menu, &QMenu::aboutToShow, std::bind(_nm_menu_inject, _this, menu, loc, menu->actions().count()));

    return AbstractNickelMenuController_createMenuTextItem(_this, menu, label, checkable, checked, thingy);
}

QString nm_menu_pixmap(const char *custom, const char *custom_temp_out, const char *fallback) {
    if (!custom || !custom_temp_out)
        return QString(fallback);

    QImage a;
    if (!a.load(QString(custom))) {
        NM_LOG("nm_menu_pixmap: error loading '%s', falling back to '%s': %s", custom, fallback, QFile::exists(custom) ? "failed to load image" : "image does not exist");
        return QString(fallback);
    }

    // The stock tab buttons come without a fallback to size against, which
    // left their custom icons unapplied. Every tab icon has the size of the
    // More icon, so size against that when no fallback is given.
    const char *reference = fallback ? fallback : ":/images/home/main_nav_more.png";
    QPixmap b;
    if (!b.load(QString(reference))) {
        NM_LOG("nm_menu_pixmap: error loading default pixmap '%s': %s", reference, QFile::exists(reference) ? "failed to load image" : "image does not exist");
        return QString(fallback);
    }

    QImage c = a.scaled(b.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    NM_LOG("nm_menu_pixmap: resized '%s' to match '%s' (%dx%d): %dx%d", custom, reference, b.size().width(), b.size().height(), c.size().width(), c.size().height());

    if (!c.save(QString(custom_temp_out), "PNG")) {
        NM_LOG("nm_menu_pixmap: error saving resized pixmap to '%s'", custom_temp_out);
        return QString(fallback);
    }

    return QString(custom_temp_out);
}

static const char *nm_main_menu_config(int index, const char *option) {
    char buf[strlen("menu_main_15505_9_icon_active") + 1];
    if (snprintf(buf, sizeof(buf), "menu_main_15505_%d_%s", index, option) >= static_cast<ssize_t>(sizeof(buf))) {
        NM_LOG("Failed to create main menu config key for index %d, option %s", index, option);
        return nullptr;
    }
    return nm_global_config_experimental(buf);
}

static void main_nav_button_configure(MainNavButton *btn, const char *label, const char *icon, const char *icon_fallback, const char *icon_active, const char *icon_active_fallback) {
    if (label && MainNavButton_setText) {
        MainNavButton_setText(btn, label);
    }

    if ((icon || icon_fallback) && MainNavButton_setPixmap) {
        QString pixmap = nm_menu_pixmap(
            icon,
            "/tmp/nm_menu.png",
            icon_fallback
        );
        if (!pixmap.isEmpty()) {
            MainNavButton_setPixmap(btn, pixmap);
        }
    }

    if ((icon_active || icon_active_fallback) && MainNavButton_setActivePixmap) {
        QString pixmap = nm_menu_pixmap(
            icon_active,
            "/tmp/nm_menu.png",
            icon_active_fallback
        );
        if (!pixmap.isEmpty()) {
            MainNavButton_setActivePixmap(btn, pixmap);
        }
    }

    if (icon || icon_fallback || icon_active || icon_active_fallback) {
        QFile::remove("/tmp/nm_menu.png");
    }
}

extern "C" __attribute__((visibility("default"))) void _nm_menu_hook2(MainNavView *_this, QWidget *parent) {
    NM_LOG("MainNavView::MainNavView(%p, %p)", _this, parent);
    MainNavView_MainNavView(_this, parent);

    if (!MainNavButton_MainNavButton || !MainNavButton_setText || !MainNavButton_tapped || !NickelTouchMenu_NickelTouchMenu || !MenuTextItem_MenuTextItem || !MenuTextItem_setText || !MenuTextItem_registerForTapGestures) {
        NM_LOG("Could not find required MainNavButton symbols, cannot add tab button for NickelMenu main menu.");
        return;
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    if (!nh_native_type("MainNavButton").isValid() || !nh_native_type("NickelTouchMenu").isValid()
            || !nh_native_type("MenuTextItem").isValid()) {
        NM_LOG("native menu type unavailable, cannot add NickelMenu tab");
        return;
    }

#endif

    QHBoxLayout *bl = _this->findChild<QHBoxLayout*>();
    if (!bl) {
        NM_LOG("Could not find QHBoxLayout(should contain MainNavButtons and be contained in the QVBoxLayout of MainNavView) in MainNavView, cannot add tab button for NickelMenu main menu.");
        return;
    }

    NM_LOG("Default main menu has %d buttons", bl->count());
    for (int i = 0; i < bl->count(); ++i) {
        QWidget *widget = bl->itemAt(i)->widget();
        if (!widget || !widget->inherits("MainNavButton")) continue;
        NM_LOG("Main menu button %d = %s", i, widget->objectName().toUtf8().constData());

        MainNavButton *btn = widget;
        if (!btn) {
            NM_LOG("qobject_cast<MainNavButton*> failed on button %d", i);
            continue;
        }

        const char *label = nm_main_menu_config(i, "label");
        const char *icon = nm_main_menu_config(i, "icon");
        const char *icon_active = nm_main_menu_config(i, "icon_active");

        const char *enabled = nm_main_menu_config(i, "enabled");
        main_nav_button_configure(btn, label, icon, nullptr, icon_active, nullptr);
        if (enabled) {
            if (strcmp("0", enabled) == 0) {
                NM_LOG("Main menu button %d disabled", i);
                widget->hide();
            } else if (strcmp("1", enabled) == 0) {
                NM_LOG("Main menu button %d explicitly enabled", i);
                widget->show();
            }
        }
    }

    const char *enabled = nm_global_config_experimental("menu_main_15505_enabled");
    if (enabled && strcmp("0", enabled) == 0) {
        NM_LOG("Main menu NickelMenu button disabled");
        return;
    }

    NM_LOG("Adding main menu button in tab bar for firmware 4.23.15505+.");
    MainNavButton *btn = reinterpret_cast<MainNavButton*>(nm_native_storage("MainNavButton", 256));
    if (!btn) {
        NM_LOG("Failed to allocate memory for MainNavButton, cannot add tab button for NickelMenu main menu.");
        return;
    }

    MainNavButton_MainNavButton(btn, parent);
    main_nav_button_configure(btn,
        nm_global_config_experimental("menu_main_15505_label") ?: "NickelMenu",
        nm_global_config_experimental("menu_main_15505_icon"),
        ":/images/home/main_nav_more.png",
        nm_global_config_experimental("menu_main_15505_icon_active"),
        ":/images/home/main_nav_more_active.png"
    );
    btn->setObjectName("nmButton");

    QPushButton *sh = new QPushButton(_this); // HACK: we use a QPushButton as an adaptor so we can connect an old-style signal with the new-style connect without needing a custom QObject
    if (!QWidget::connect(btn, SIGNAL(tapped()), sh, SIGNAL(pressed()))) {
        NM_LOG("Failed to connect SIGNAL(tapped()) on TouchLabel to SIGNAL(pressed()) on the QPushButton shim, cannot add tab button for NickelMenu main menu.");
        return;
    }
    sh->setVisible(false);

    QWidget::connect(sh, &QPushButton::pressed, [btn] {
        if (!NickelTouchMenu_NickelTouchMenu || !MenuTextItem_MenuTextItem || !MenuTextItem_setText || !MenuTextItem_registerForTapGestures) {
            NM_LOG("could not find required NickelTouchMenu and MenuTextItem symbols for generating menu");
            nm_show_dialog(QLatin1String("NickelMenu"), QLatin1String("Could not find required NickelTouchMenu and MenuTextItem symbols for generating menu (this is a bug)."));
            return;
        }

        NM_LOG("checking for config updates");
        int rev = nm_global_config_update();
        NM_LOG("revision = %d", rev);

        NM_LOG("building menu");

        size_t items_n;
        nm_menu_item_t **items = nm_global_config_items(&items_n);

        if (!items) {
            NM_LOG("failed to get menu items");
            nm_show_dialog(QLatin1String("NickelMenu"), QLatin1String("Failed to get menu items (this might be a bug)."));
            return;
        }

        NickelTouchMenu *menu = reinterpret_cast<NickelTouchMenu*>(nm_native_storage("NickelTouchMenu", 512));
        if (!menu) {
            NM_LOG("failed to allocate memory for menu");
            nm_show_dialog(QLatin1String("NickelMenu"), QLatin1String("Failed to allocate memory for menu."));
            return;
        }

        NickelTouchMenu_NickelTouchMenu(menu, nullptr, 3);

        for (size_t i = 0; i < items_n; i++) {
            nm_menu_item_t *it = items[i];
            if (it->loc != NM_MENU_LOCATION(main))
                continue;

            NM_LOG("adding item '%s'...", it->lbl);

            // based on _ZN23SelectionMenuController18createMenuTextItemEP7QWidgetRK7QString
            // (also see _ZN28AbstractNickelMenuController18createMenuTextItemEP5QMenuRK7QStringbbS4_, which seems to do the gestures itself instead of calling registerForTapGestures)

            MenuTextItem *mti = reinterpret_cast<MenuTextItem*>(nm_native_storage("MenuTextItem", 256));
            if (!mti) {
                NM_LOG("failed to allocate memory for config item");
                menu->deleteLater();
                nm_show_dialog(QLatin1String("NickelMenu"), QLatin1String("Failed to allocate memory for menu item."));
                return;
            }

            MenuTextItem_MenuTextItem(mti, menu, false, true);
            MenuTextItem_setText(mti, QString::fromUtf8(it->lbl));
            MenuTextItem_registerForTapGestures(mti); // this only makes the MenuTextItem::tapped signal connect so it highlights on tap, doesn't apply to the QAction::triggered below (which needs another GestureReceiver somewhere)

            // based on _ZN22AbstractMenuController12createActionEP5QMenuP7QWidgetbbb

            QWidgetAction *ac = new QWidgetAction(menu);
            ac->setDefaultWidget(mti);
            ac->setEnabled(true);

            menu->addAction(ac);

            QWidget::connect(ac, &QAction::triggered, menu, &QMenu::hide);

            if (i != items_n-1)
                menu->addSeparator();

            // shim so we don't need to deal with GestureReceiver directly like _ZN28AbstractNickelMenuController18createMenuTextItemEP5QMenuRK7QStringbbS4_ does
            // (similar to _ZN23SelectionMenuController11addMenuItemEP17SelectionMenuViewP12MenuTextItemPKc)

            if (!QWidget::connect(mti, SIGNAL(tapped(bool)), ac, SIGNAL(triggered()))) {
                NM_LOG("could not handle touch events for menu item (connection of SIGNAL(tapped(bool)) on MenuTextItem to SIGNAL(triggered()) on QWidgetAction failed)");
                menu->deleteLater();
                nm_show_dialog(QLatin1String("NickelMenu"), QLatin1String("Could not attach touch event handlers to menu item (this is a bug)."));
                return;
            }

            // event handler

            QObject::connect(ac, &QAction::triggered, [it](bool) {
                NM_LOG("item '%s' pressed...", it->lbl);
                nm_menu_item_do(it, NULL, NULL);
                NM_LOG("done");
            }); // note: we're capturing by value, i.e. the pointer to the global variable, rather then the stack variable, so this is safe
        }

        NM_LOG("showing menu");

        QWidget::connect(menu, &QMenu::aboutToHide, menu, &QWidget::deleteLater);

        menu->ensurePolished();
        menu->popup(btn->mapToGlobal(btn->geometry().topRight() - QPoint(0, menu->sizeHint().height())));
    });

    bl->addWidget(btn, 1);
    _this->ensurePolished();

    NM_LOG("Added button.");
}

// _nm_menu_hook4_item gets/sets the current menu item. It must only be called
// by one thread at a time. The item will be cleared after it has been used.
nm_menu_item_t *_nm_menu_hook4_item(nm_menu_item_t *it) {
    static nm_menu_item_t *its = NULL;
    if (it)
        return (its = it);
    if (!its)
        return NULL;
    nm_menu_item_t *tmp = its;
    its = NULL;
    return tmp;
}

typedef struct {
    QString const& selection;
} nm_selmenu_argtransform_data_t;
char *_nm_selmenu_argtransform(void *data, const char *arg);
static void nm_selection_inject(SelectionMenuController *, SelectionMenuView *, nm_menu_location_t);

extern "C" __attribute__((visibility("default"))) void _nm_selection_search(SelectionMenuController *_this, bool full) {
    SelectionMenuController_setupSearchOptions(_this, full);
    // Firmware 6 adds web search rows directly, bypassing the controller hook.
    if (!SelectionMenuController_lookupWikipedia && SelectionMenuController_menuView)
        nm_selection_inject(_this, SelectionMenuController_menuView(_this), NM_MENU_LOCATION(selection_search));
}

extern "C" __attribute__((visibility("default"))) void _nm_menu_hook3(SelectionMenuController *_this, SelectionMenuView* smv, MenuTextItem* mti, const char *slot) {
    NM_LOG("hook3: %p %p %p %s", _this, smv, mti, slot);
    SelectionMenuController_addMenuItem(_this, smv, mti, slot);

    // this is important for another reason other than positioning: it only displays if Volume::canSearch()
    nm_menu_location_t loc;
    if (!strcmp(slot, "1showSearchOptions()")) //libnickel 4.20.14622 * _ZN23SelectionMenuController17showSearchOptionsEv
        loc = NM_MENU_LOCATION(selection);
    else if (!strcmp(slot, "1onSearchInBook()")) //libnickel 4.20.14601 * _ZN23SelectionMenuController14onSearchInBookEv
        loc = NM_MENU_LOCATION(selection); // this one is shown instead if parental controls are enabled
    else if (!strcmp(slot, "2lookupWeb()")) //libnickel 4.20.14622 4.30.18838 _ZN23SelectionMenuController9lookupWebEv
        loc = NM_MENU_LOCATION(selection_search);
    else if (!strcmp(slot, "2lookupGoogle()")) //libnickel 4.31.19086 * _ZN23SelectionMenuController12lookupGoogleEv _ZN23SelectionMenuController12lookupGoogleERK7QString _ZN23SelectionMenuController9lookupWebEv
        loc = NM_MENU_LOCATION(selection_search);
    else if (!strcmp(slot, "2lookupBaidu()")) //libnickel 4.31.19086 * _ZN23SelectionMenuController11lookupBaiduEv _ZN23SelectionMenuController11lookupBaiduERK7QString
        loc = NM_MENU_LOCATION(selection_search);
    else
        return;

    nm_selection_inject(_this, smv, loc);
}

static void nm_selection_inject(SelectionMenuController *_this, SelectionMenuView *smv, nm_menu_location_t loc) {
    const bool modern = !SelectionMenuController_lookupWikipedia;
    if (!smv || !SelectionMenuView_addMenuItem || !MenuTextItem_MenuTextItem || !MenuTextItem_setText || !MenuTextItem_registerForTapGestures
            || (modern ? (!PageImageReader_selectedText || !SelectionMenuController_close || !SelectionMenuController_clearSelection)
                       : !WebSearchMixinBase_doWikipediaSearch)) {
        NM_LOG("selection menu dependencies unavailable");
        return;
    }

    NM_LOG("checking for config updates");
    int rev = nm_global_config_update();
    NM_LOG("revision = %d", rev);

    NM_LOG("adding items");

    size_t items_n;
    nm_menu_item_t **items = nm_global_config_items(&items_n);

    if (!items) {
        NM_LOG("failed to get menu items");
        nm_show_dialog(QLatin1String("NickelMenu"), QLatin1String("Failed to get menu items (this might be a bug)."));
        return;
    }

    for (size_t i = 0; i < items_n; i++) {
        nm_menu_item_t *it = items[i];
        if (it->loc != loc)
            continue;

        NM_LOG("adding item '%s'...", it->lbl);

        // based on _ZN23SelectionMenuController18createMenuTextItemEP7QWidgetRK7QString

        MenuTextItem *mti = reinterpret_cast<MenuTextItem*>(nm_native_storage("MenuTextItem", 256));
        if (!mti) {
            NM_LOG("failed to allocate memory for config item");
            return;
        }

        MenuTextItem_MenuTextItem(mti, smv, false, true);
        MenuTextItem_setText(mti, QString::fromUtf8(it->lbl));
        MenuTextItem_registerForTapGestures(mti);

        // based on _ZN23SelectionMenuController18setupSearchOptionsEb and _ZN23SelectionMenuController11addMenuItemEP17SelectionMenuViewP12MenuTextItemPKc, plus some custom stuff for better compatibility

        QPushButton *sh = new QPushButton(smv); // HACK: we use a QPushButton as an adaptor so we can connect an old-style signal with the new-style connect without needing a custom QObject
        if (!QWidget::connect(mti, SIGNAL(tapped(bool)), sh, SIGNAL(pressed()))) {
            NM_LOG("Failed to connect SIGNAL(tapped(bool)) on MenuTextItem to SIGNAL(pressed()) on the QPushButton shim, cannot add custom selection menu item.");
            return;
        }
        sh->setVisible(false);

        QObject::connect(sh, &QPushButton::pressed, [_this, it]() {
            NM_LOG("item '%s' pressed...", it->lbl);
            if (!SelectionMenuController_lookupWikipedia) {
                // Firmware 6 keeps the live selection in the reader, while the
                // current Bookmark can still have empty text. Use the same
                // accessor as its native search menu, without private offsets.
                QWidget *reader = nullptr;
                for (auto *widget : QApplication::allWidgets()) {
                    if (!widget->isVisible() || !widget->inherits("PageImageReader")) continue;
                    if (reader) {
                        nm_show_dialog(QStringLiteral("NickelMenu"), QStringLiteral("More than one active reader."));
                        return;
                    }
                    reader = widget;
                }
                if (!reader || !PageImageReader_selectedText) {
                    nm_show_dialog(QStringLiteral("NickelMenu"), QStringLiteral("Selected text is unavailable."));
                    return;
                }
                const QString selection = PageImageReader_selectedText(reader);
                SelectionMenuController_close(_this);
                SelectionMenuController_clearSelection(_this);
                nm_selmenu_argtransform_data_t data{selection};
                nm_menu_item_do(it, _nm_selmenu_argtransform, &data);
                return;
            }
            _nm_menu_hook4_item(it); // this is safe since it is a pointer captured by value
            NM_LOG("triggering lookupWikipedia() slot");
            SelectionMenuController_lookupWikipedia(_this);
        });

        SelectionMenuView_addMenuItem(smv, mti);
    }
}

char *_nm_selmenu_argtransform(void *data, const char *arg) {
    nm_selmenu_argtransform_data_t *d = (nm_selmenu_argtransform_data_t*)(data);

    QString src = QString::fromUtf8(arg), res;
    QRegularExpression re = QRegularExpression("\\{([1])\\|([aAfnsSuwx]*)\\|([\"$%]*)\\}");

    for (QString x = src; x.length() > 0;) {
        QRegularExpressionMatch m = re.match(x);

        if (!m.hasMatch()) {
            res += x;
            x = x.mid(x.length());
            continue;
        }

        QString tmp;

        for (int k = 0; k < m.capturedLength(1); k++) {
            switch (m.captured(1).at(k).toLatin1()) {
            case '1': tmp = d->selection; break;
            }
        }

        for (int k = 0; k < m.capturedLength(2); k++) {
            switch (m.captured(2).at(k).toLatin1()) {
            case 'a': tmp = tmp.toLower(); break;
            case 'A': tmp = tmp.toUpper(); break;
            case 'f': tmp = tmp.split(QRegularExpression("\\s")).first(); break;
            case 'n': tmp = tmp.remove(QRegularExpression("[^0-9a-zA-Z]")); break;
            case 's': tmp = tmp.trimmed(); break;
            case 'S': tmp = tmp.simplified(); break;
            case 'x': if (tmp.length() == 0) { nm_err_set("argtransform: empty substitution result for %s", qPrintable(m.captured(0))); return NULL; }; break;
            case 'w': tmp = tmp.remove(QRegularExpression("\\s")); break;
            case 'u': tmp = tmp.replace(QRegularExpression("\\s"), "_"); break;
            }
        }

        for (int k = 0; k < m.capturedLength(3); k++) {
            switch (m.captured(3).at(k).toLatin1()) {
            case '"':
                // Escape existing backslashes before adding escapes for other characters.
                tmp = tmp
                    .replace("\\", "\\\\")
                    .replace("\"", "\\\"")
                    .replace("\n", "\\n")
                    .replace("\b", "\\b")
                    .replace("\t", "\\t")
                    .replace("\f", "\\f")
                    .replace("\r", "\\r");
                // JSON also requires escapes for the remaining control characters.
                for (int c = 0; c < 0x20; ++c)
                    tmp.replace(QChar(c), QStringLiteral("\\u%1").arg(c, 4, 16, QLatin1Char('0')));
                break;
            case '$':
                tmp = tmp.replace("'", "'\"'\"'");
                break;
            case '%':
                tmp = QUrl::toPercentEncoding(tmp);
                break;
            }
        }

        res += x.left(m.capturedStart());
        res += tmp;
        x = x.mid(m.capturedEnd());
    }

    char *x = strdup(res.toUtf8().data());
    if (!x)
        nm_err_set("argtransform: could not allocate memory: %m");
    return x;
}

extern "C" __attribute__((visibility("default"))) void _nm_menu_hook4(WebSearchMixinBase *_this, QString const& selection, QString const& locale) {
    NM_LOG("hook4: %p %s %s", _this, qPrintable(selection), qPrintable(locale));

    nm_menu_item_t *it = _nm_menu_hook4_item(NULL);
    if (!it) {
        NM_LOG("No current menu item, continuing with default wikipedia search.");
        WebSearchMixinBase_doWikipediaSearch(_this, selection, locale);
        return;
    }

    NM_LOG("continuing execution of item %p (%s)", it, it->lbl);
    nm_selmenu_argtransform_data_t data = (nm_selmenu_argtransform_data_t){
        .selection = selection,
    };
    nm_menu_item_do(it, _nm_selmenu_argtransform, (void*)(&data)); // this is safe since data will not be used after this returns
    NM_LOG("done");
}

void _nm_menu_inject(void *nmc, QMenu *menu, nm_menu_location_t loc, int at) {
    NM_LOG("inject %d @ %d", loc, at);
    if (!AbstractNickelMenuController_createMenuTextItem || !AbstractNickelMenuController_createAction)
        return;

    int rev_o = menu->property("nm_config_rev").toInt();

    NM_LOG("checking for config updates (current revision: %d)", rev_o);
    int rev_n = nm_global_config_update(); // if there was an error it will be returned as a menu item anyways (and updated will be true)
    NM_LOG("new revision = %d%s", rev_n, rev_n == rev_o ? "" : " (changed)");

    NM_LOG("checking for existing items added by nm");

    for (auto action : menu->actions()) {
        if (action->property("nm_action") == true) {
            if (rev_o == rev_n)
                return; // already added items, menu is up to date
            menu->removeAction(action);
            delete action;
        }
    }

    NM_LOG("getting insertion point");

    auto actions = menu->actions();
    auto before = at < actions.count()
        ? actions.at(at)
        : nullptr;

    if (before == nullptr)
        NM_LOG("it seems the original item to add new ones before was never actually added to the menu (number of items when the action was created is %d, current is %d), appending to end instead", at, actions.count());

    NM_LOG("injecting new items");

    size_t items_n;
    nm_menu_item_t **items = nm_global_config_items(&items_n);

    if (!items) {
        NM_LOG("items is NULL (either the config hasn't been parsed yet or there was a memory allocation error), not adding");
        return;
    }

    // if it segfaults in createMenuTextItem, it's likely because
    // AbstractNickelMenuController is invalid, which shouldn't happen while the
    // menu which we added the signal from still can be shown... (but
    // theoretically, it's possible)

    for (size_t i = 0; i < items_n; i++) {
        nm_menu_item_t *it = items[i];
        if (it->loc != loc)
            continue;

        NM_LOG("adding item '%s'...", it->lbl);

        MenuTextItem* item = AbstractNickelMenuController_createMenuTextItem(nmc, menu, QString::fromUtf8(it->lbl), false, false, "");
        if (!item) continue;
        QAction* action = AbstractNickelMenuController_createAction_before(before, loc, i == items_n-1, nmc, menu, item, true, true, true);

        if (!action) { item->deleteLater(); continue; }
        QObject::connect(action, &QAction::triggered, [it](bool){
            NM_LOG("item '%s' pressed...", it->lbl);
            nm_menu_item_do(it, NULL, NULL);
            NM_LOG("done");
        }); // note: we're capturing by value, i.e. the pointer to the global variable, rather then the stack variable, so this is safe
    }

    NM_LOG("updating config revision property");
    menu->setProperty("nm_config_rev", rev_n);
}

void nm_menu_item_do(nm_menu_item_t *it, nm_argtransform_t argtransform, void *argtransform_data) {
    const char *err = NULL;
    bool success = true;
    int skip = 0;

    for (nm_menu_action_t *cur = it->action; cur; cur = cur->next) {
        NM_LOG("action %p with argument %s : ", cur->act, cur->arg);
        NM_LOG("...success=%d ; on_success=%d on_failure=%d skip=%d", success, cur->on_success, cur->on_failure, skip);

        if (skip != 0) {
            NM_LOG("...skipping action due to skip flag (remaining=%d)", skip);
            if (skip > 0)
                skip--;
            continue;
        } else if (!((success && cur->on_success) || (!success && cur->on_failure))) {
            NM_LOG("...skipping action due to condition flags");
            continue;
        }

        nm_action_result_t *res = NULL;
        if (!argtransform) {
            res = cur->act(cur->arg);
        } else {
            NM_LOG("...applying argtransform");
            char *arg = (*argtransform)(argtransform_data, cur->arg);
            if (arg) {
                NM_LOG("...applied argtransform: %s", arg);
                res = cur->act(arg);
                free(arg);
            }
        }
        err = nm_err();

        if (err == NULL && res && res->type == NM_ACTION_RESULT_TYPE_SKIP) {
            NM_LOG("...not updating success flag (value=%d) for skip result", success);
        } else if (!(success = err == NULL)) {
            NM_LOG("...error: '%s'", err);
            continue;
        } else if (!res) {
            NM_LOG("...warning: you should have returned a result with type silent, not null, upon success");
            continue;
        }

        NM_LOG("...result: type=%d msg='%s', handling...", res->type, res->msg);

        MainWindowController *mwc;
        switch (res->type) {
        case NM_ACTION_RESULT_TYPE_SILENT:
            break;
        case NM_ACTION_RESULT_TYPE_MSG:
            nm_show_dialog(QString::fromUtf8(it->lbl), QString::fromUtf8(res->msg));
            break;
        case NM_ACTION_RESULT_TYPE_TOAST:
            if (!MainWindowController_sharedInstance || !MainWindowController_toast) {
                NM_LOG("toast unavailable: %s", res->msg);
                break;
            }
            mwc = MainWindowController_sharedInstance();
            if (!mwc) {
                NM_LOG("toast: could not get shared main window controller pointer");
                break;
            }
            MainWindowController_toast(mwc, QString::fromUtf8(res->msg), QStringLiteral(""), 1500);
            break;
        case NM_ACTION_RESULT_TYPE_SKIP:
            skip = res->skip;
            break;
        }

        if (skip == -1)
            NM_LOG("...skipping remaining actions");
        else if (skip != 0)
            NM_LOG("...skipping next %d actions", skip);

        nm_action_result_free(res);
    }

    if (err) {
        NM_LOG("last action returned error %s", err);
        nm_show_dialog(QString::fromUtf8(it->lbl), QString::fromUtf8(err));
    }
}

QAction *AbstractNickelMenuController_createAction_before(QAction *before, nm_menu_location_t loc, bool last_in_group, void *_this, QMenu *menu, QWidget *widget, bool close, bool enabled, bool separator) {
    int n = menu->actions().count();
    QAction* action = AbstractNickelMenuController_createAction(_this, menu, widget, /*close*/false, enabled, /*separator*/false);

    if (!action) return nullptr;
    action->setProperty("nm_action", true);

    if (!menu->actions().contains(action)) {
        NM_LOG("could not find added action at end of menu (note: old count is %d, new is %d), not moving it to the right spot or adding separator", n, menu->actions().count());
        return action;
    }

    if (before != nullptr) {
        menu->removeAction(action);
        menu->insertAction(before, action);
    }

    if (close) {
        // we can't use the signal which createAction can create, as it gets lost when moving the item
        QWidget::connect(action, &QAction::triggered, [=](bool){ menu->hide(); });
    }

    if (separator) {
        // if it's the main menu, we generally want to use a custom separator
        QAction *sep;
        if (loc == NM_MENU_LOCATION(main) && LightMenuSeparator_LightMenuSeparator && BoldMenuSeparator_BoldMenuSeparator) {
            sep = reinterpret_cast<QAction*>(nm_native_storage(last_in_group ? "BoldMenuSeparator" : "LightMenuSeparator", 32));
            if (!sep) return action;
            (last_in_group
                ? BoldMenuSeparator_BoldMenuSeparator
                : LightMenuSeparator_LightMenuSeparator
            )(sep, reinterpret_cast<QWidget*>(_this));
            menu->insertAction(before, sep);
        } else {
            sep = menu->insertSeparator(before);
        }
        sep->setProperty("nm_action", true);
    }

    return action;
}
