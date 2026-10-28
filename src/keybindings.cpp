#include "keybindings.h"
#include "settings.h"
#include <QSet>

KeyBindings::KeyBindings(QObject *parent) : QAbstractListModel(parent)
{
#ifdef Q_OS_MACOS
    const QKeySequence fullscreen(QStringLiteral("Ctrl+Meta+F"));
    const QKeySequence developerToolsKey(QStringLiteral("Ctrl+Alt+I"));
    const QKeySequence downloadsKey(QStringLiteral("Ctrl+Shift+J"));
    const QString tabModifier = QStringLiteral("Meta+");
    const QKeySequence back(QStringLiteral("Ctrl+Left")), forward(QStringLiteral("Ctrl+Right"));
    const QKeySequence pageTop(QStringLiteral("Ctrl+Up")), pageBottom(QStringLiteral("Ctrl+Down"));
#else
    const QKeySequence fullscreen(QStringLiteral("F11"));
    const QKeySequence developerToolsKey(QStringLiteral("Ctrl+Shift+I"));
    const QKeySequence downloadsKey(QStringLiteral("Ctrl+J"));
    const QString tabModifier = QStringLiteral("Ctrl+");
    const QKeySequence back(QKeySequence::Back), forward(QKeySequence::Forward);
    const QKeySequence pageTop(QStringLiteral("Ctrl+Home")), pageBottom(QStringLiteral("Ctrl+End"));
#endif
    const auto add = [this](const char *id, const char *label, const QKeySequence &key) {
        m_bindings.push_back({QString::fromLatin1(id), tr(label), key, key});
    };
    add("address", QT_TRANSLATE_NOOP("KeyBindings", "Edit address"), QKeySequence(QStringLiteral("Ctrl+L")));
    add("showNavigation", QT_TRANSLATE_NOOP("KeyBindings", "Show navigation pill"), QKeySequence(QStringLiteral("Ctrl+Shift+L")));
    add("sharePage", QT_TRANSLATE_NOOP("KeyBindings", "Share current page"), QKeySequence(QStringLiteral("Ctrl+Shift+S")));
    add("developerTools", QT_TRANSLATE_NOOP("KeyBindings", "Toggle developer tools for focused view"), developerToolsKey);
    add("downloads", QT_TRANSLATE_NOOP("KeyBindings", "Show downloads"), downloadsKey);
    add("print", QT_TRANSLATE_NOOP("KeyBindings", "Print current page"), QKeySequence(QKeySequence::Print));
    add("extensions", QT_TRANSLATE_NOOP("KeyBindings", "Show extensions"), QKeySequence(QStringLiteral("Ctrl+Shift+X")));
    add("settings", QT_TRANSLATE_NOOP("KeyBindings", "Settings"), QKeySequence(QStringLiteral("Ctrl+,")));
    add("shortcutHelp", QT_TRANSLATE_NOOP("KeyBindings", "Show shortcut help"), QKeySequence(QStringLiteral("Ctrl+/")));
    add("passwords", QT_TRANSLATE_NOOP("KeyBindings", "Fill or save login"), QKeySequence(QStringLiteral("Ctrl+Shift+P")));
    add("reload", QT_TRANSLATE_NOOP("KeyBindings", "Reload page"), QKeySequence(QKeySequence::Refresh));
    add("back", QT_TRANSLATE_NOOP("KeyBindings", "Go back"), back);
    add("forward", QT_TRANSLATE_NOOP("KeyBindings", "Go forward"), forward);
    add("pageTop", QT_TRANSLATE_NOOP("KeyBindings", "Go to top of page"), pageTop);
    add("pageBottom", QT_TRANSLATE_NOOP("KeyBindings", "Go to bottom of page"), pageBottom);
    add("stop", QT_TRANSLATE_NOOP("KeyBindings", "Stop loading"), QKeySequence(Qt::Key_Escape));
    add("expand", QT_TRANSLATE_NOOP("KeyBindings", "Expand / restore window"), QKeySequence(QStringLiteral("Ctrl+Meta+M")));
    add("fullscreen", QT_TRANSLATE_NOOP("KeyBindings", "Enter / exit full screen"), fullscreen);
    add("close", QT_TRANSLATE_NOOP("KeyBindings", "Close tab"), QKeySequence(QKeySequence::Close));
    add("privateWindow", QT_TRANSLATE_NOOP("KeyBindings", "New private window"), QKeySequence(QStringLiteral("Ctrl+Shift+N")));
    add("lockPrivate", QT_TRANSLATE_NOOP("KeyBindings", "Lock private window"), QKeySequence(QStringLiteral("Ctrl+Alt+L")));
    add("newTab", QT_TRANSLATE_NOOP("KeyBindings", "New tab"), QKeySequence(QStringLiteral("Ctrl+T")));
    add("tabs", QT_TRANSLATE_NOOP("KeyBindings", "Show tabs"), QKeySequence(QStringLiteral("Ctrl+Shift+A")));
    add("nextTab", QT_TRANSLATE_NOOP("KeyBindings", "Next tab"), QKeySequence(tabModifier + QStringLiteral("Tab")));
    add("previousTab", QT_TRANSLATE_NOOP("KeyBindings", "Previous tab"), QKeySequence(tabModifier + QStringLiteral("Shift+Tab")));
    add("reopenTab", QT_TRANSLATE_NOOP("KeyBindings", "Reopen closed tab"), QKeySequence(QStringLiteral("Ctrl+Shift+T")));
    add("closeWindow", QT_TRANSLATE_NOOP("KeyBindings", "Close window"), QKeySequence(QStringLiteral("Ctrl+Shift+W")));
    add("workspaces", QT_TRANSLATE_NOOP("KeyBindings", "Show workspaces"), QKeySequence(QStringLiteral("Ctrl+Shift+E")));
    add("nextWorkspace", QT_TRANSLATE_NOOP("KeyBindings", "Next workspace"), QKeySequence(QStringLiteral("Ctrl+Alt+Right")));
    add("previousWorkspace", QT_TRANSLATE_NOOP("KeyBindings", "Previous workspace"), QKeySequence(QStringLiteral("Ctrl+Alt+Left")));
    add("splitHorizontal", QT_TRANSLATE_NOOP("KeyBindings", "Split left / right"), QKeySequence(QStringLiteral("Ctrl+Alt+S")));
    add("splitVertical", QT_TRANSLATE_NOOP("KeyBindings", "Split top / bottom"), QKeySequence(QStringLiteral("Ctrl+Alt+V")));
    add("splitWith", QT_TRANSLATE_NOOP("KeyBindings", "Split with a tab from any workspace"), QKeySequence(QStringLiteral("Ctrl+Alt+Shift+S")));
    add("nextPane", QT_TRANSLATE_NOOP("KeyBindings", "Focus next pane"), QKeySequence(QStringLiteral("Ctrl+Alt+]")));
    add("previousPane", QT_TRANSLATE_NOOP("KeyBindings", "Focus previous pane"), QKeySequence(QStringLiteral("Ctrl+Alt+[")));
    add("nextHiddenTab", QT_TRANSLATE_NOOP("KeyBindings", "Show next hidden tab in active pane"), QKeySequence(QStringLiteral("Ctrl+Alt+N")));
    add("toggleAdBlock", QT_TRANSLATE_NOOP("KeyBindings", "Toggle ad blocking in focused view"), QKeySequence(QStringLiteral("Ctrl+Alt+B")));
    add("history", QT_TRANSLATE_NOOP("KeyBindings", "Show history"), QKeySequence(QStringLiteral("Ctrl+Y")));
    Settings settings;
    QSet<QKeySequence> reserved;
    for(const auto &binding:m_bindings) if(binding.id!="shortcutHelp") {
        reserved.insert(binding.fallback);
        reserved.insert(QKeySequence::fromString(settings.value("KeyboardBindings/"+binding.id,
            binding.fallback.toString(QKeySequence::PortableText)).toString(),QKeySequence::PortableText));
    }
    QSet<QKeySequence> used;
    // Load as one set: invalid or duplicate saved shortcuts revert the whole set.
    bool valid = true;
    for (auto &binding : m_bindings) {
        auto initial=binding.fallback;
        // An existing user's Ctrl+/ assignment must survive this new action.
        if(binding.id=="shortcutHelp" && !settings.contains("KeyboardBindings/shortcutHelp") && reserved.contains(initial))
            for(int alternative=0;alternative<70;++alternative) {
                const auto key=static_cast<Qt::Key>(Qt::Key_F1+alternative%35);
                const QKeySequence candidate(QKeyCombination(alternative<35 ? Qt::NoModifier
                    : Qt::ControlModifier|Qt::AltModifier|Qt::ShiftModifier,key));
                if(!reserved.contains(candidate)) { initial=candidate; break; }
            }
        binding.value = QKeySequence::fromString(settings.value(
            QStringLiteral("KeyboardBindings/") + binding.id,
            initial.toString(QKeySequence::PortableText)).toString(), QKeySequence::PortableText);
        if (binding.value.isEmpty() || binding.value.count() != 1 || used.contains(binding.value))
            valid = false;
        used.insert(binding.value);
    }
    if (!valid)
        for (auto &binding : m_bindings) binding.value = binding.fallback;
}

int KeyBindings::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : m_bindings.size(); }
QVariant KeyBindings::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_bindings.size()) return {};
    const auto &binding = m_bindings[index.row()];
    switch (role) {
    case Qt::UserRole: return binding.id;
    case Qt::UserRole + 1: return binding.label;
    case Qt::UserRole + 2: return binding.value.toString(QKeySequence::NativeText);
    default: return {};
    }
}
QHash<int, QByteArray> KeyBindings::roleNames() const
{
    return {{Qt::UserRole, "actionId"}, {Qt::UserRole + 1, "actionLabel"}, {Qt::UserRole + 2, "keyLabel"}};
}
QVariantMap KeyBindings::bindings() const
{
    QVariantMap result;
    for (const auto &binding : m_bindings)
        result.insert(binding.id, binding.value.toString(QKeySequence::PortableText));
    return result;
}
QString KeyBindings::nativeSequence(const QString &sequence) const
{
    return QKeySequence::fromString(sequence, QKeySequence::PortableText).toString(QKeySequence::NativeText);
}
QVariantList KeyBindings::commonShortcuts() const
{
    const QStringList actions{"address","newTab","tabs","nextTab","previousTab","close","reopenTab","workspaces",
        "splitHorizontal","splitVertical","nextPane","nextHiddenTab","showNavigation","history","settings","shortcutHelp"};
    QVariantList result;
    for(const auto &action:actions)
        for(const auto &binding:m_bindings)
            if(binding.id==action) {
                result.append(QVariantMap{{"actionId",binding.id},{"actionLabel",binding.label},
                    {"keyLabel",binding.value.toString(QKeySequence::NativeText)}});
                break;
            }
    return result;
}
QString KeyBindings::assign(int row, int key, int modifiers)
{
    if (row < 0 || row >= m_bindings.size()) return tr("Unknown action.");
    if (key == Qt::Key_unknown || key == Qt::Key_Shift || key == Qt::Key_Control
        || key == Qt::Key_Meta || key == Qt::Key_Alt || key == Qt::Key_AltGr)
        return tr("Keep holding the modifiers and press a key.");
    const auto mods = Qt::KeyboardModifiers(modifiers) & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
    if (!(mods & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))
        && !(key >= Qt::Key_F1 && key <= Qt::Key_F35) && key != Qt::Key_Escape)
        return tr("Use Command, Control, or Option/Alt with a key, or a function key.");
    const QKeySequence sequence(QKeyCombination(mods, static_cast<Qt::Key>(key)));
    for (int i = 0; i < m_bindings.size(); ++i)
        if (i != row && m_bindings[i].value == sequence)
            return tr("Already assigned to %1.").arg(m_bindings[i].label);
    m_bindings[row].value = sequence;
    save();
    emit dataChanged(index(row), index(row));
    emit bindingsChanged();
    return {};
}
void KeyBindings::resetDefaults()
{
    for (auto &binding : m_bindings) binding.value = binding.fallback;
    save();
    emit dataChanged(index(0), index(m_bindings.size() - 1));
    emit bindingsChanged();
}
void KeyBindings::save()
{
    Settings settings;
    for (const auto &binding : m_bindings)
        settings.setValue(QStringLiteral("KeyboardBindings/") + binding.id,
                          binding.value.toString(QKeySequence::PortableText));
    settings.sync();
}
