#ifndef COMPOSE_H
#define COMPOSE_H

#include <QObject>
#include <QMetaObject>
#include <QPointer>

class QPlainTextEdit;
class QGridLayout;
class QWidget;
class QEvent;
class TabWidget;
class TermWidgetImpl;
class TermWidget;
class TermWidgetHolder;

class ComposeInput : public QObject
{
    Q_OBJECT

public:
    ComposeInput(QWidget *container, QGridLayout *layout, TabWidget *tabulator, QObject *parent = nullptr);

    bool isActive() const { return m_active; }
    QPlainTextEdit *editor() { return m_editor; }

    void updateHeight();
    void setRawInputMode(bool raw);
    void send();
    void transferToTerminal();
    void transferFromTerminal();
    void focusTerminal();

    bool viewportEventFilter(QObject *watched, QEvent *event);
    TermWidgetImpl *currentImpl();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    enum class Cli { Claude, Gemini, Codex, Unknown };
    TermWidget *currentTermWidget();
    static Cli detectCli(TermWidgetImpl *impl);
    static void sendCtrlKey(TermWidgetImpl *impl, int key);
    static void sendKey(TermWidgetImpl *impl, int key);
    void clearTerminalInput(TermWidgetImpl *impl);
    QString normalizeSelection(const QString &text) const;
    void scheduleOverlayRelayout();
    void relayoutOverlay();
    void refreshOverlayTargets();
    void setTrackedTerm(TermWidget *term);

    QWidget *m_container = nullptr;
    TabWidget *m_tabulator;
    QPointer<TermWidgetHolder> m_trackedHolder;
    QPointer<TermWidget> m_trackedTerm;
    QMetaObject::Connection m_holderFocusConnection;
    QPlainTextEdit *m_editor = nullptr;
    bool m_overlayRelayoutPending = false;
    bool m_active = false;
    bool m_rawMode = false;
    bool m_submitInProgress = false;
};

#endif
