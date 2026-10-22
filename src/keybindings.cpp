#include "keybindings.h"
#include <QSettings>
#include <QSet>

KeyBindings::KeyBindings(QObject *parent) : QAbstractListModel(parent)
{
#ifdef Q_OS_MACOS
    const QKeySequence fullscreen(QStringLiteral("Ctrl+Meta+F"));
#else
    const QKeySequence fullscreen(QStringLiteral("F11"));
#endif
    const auto add = [this](const char *id, const char *label, const QKeySequence &key) {
        m_bindings.push_back({QString::fromLatin1(id), tr(label), key, key});
    };
    add("address", "Edit address", QKeySequence(QStringLiteral("Ctrl+L")));
    add("settings", "Settings", QKeySequence(QStringLiteral("Ctrl+,")));
    add("reload", "Reload page", QKeySequence(QKeySequence::Refresh));
    add("back", "Go back", QKeySequence(QKeySequence::Back));
    add("forward", "Go forward", QKeySequence(QKeySequence::Forward));
    add("stop", "Stop loading", QKeySequence(Qt::Key_Escape));
    add("expand", "Expand / restore window", QKeySequence(QStringLiteral("Ctrl+Meta+M")));
    add("fullscreen", "Enter / exit full screen", fullscreen);
    add("close", "Close window", QKeySequence(QKeySequence::Close));
    QSettings settings;
    QSet<QKeySequence> used;
    // Load as one set: invalid or duplicate saved shortcuts revert the whole set.
    bool valid = true;
    for (auto &binding : m_bindings) {
        binding.value = QKeySequence::fromString(settings.value(
            QStringLiteral("KeyboardBindings/") + binding.id,
            binding.fallback.toString(QKeySequence::PortableText)).toString(), QKeySequence::PortableText);
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
    QSettings settings;
    for (const auto &binding : m_bindings)
        settings.setValue(QStringLiteral("KeyboardBindings/") + binding.id,
                          binding.value.toString(QKeySequence::PortableText));
    settings.sync();
}
