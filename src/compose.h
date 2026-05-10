#ifndef COMPOSE_H
#define COMPOSE_H

#include <QObject>
#include <QHash>
#include <QPointer>
#include <QRect>

class QPlainTextEdit;
class QGridLayout;
class QWidget;
class QEvent;
class TabWidget;
class TermWidgetImpl;
class QTermWidget;
class TermWidget;

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
    void onHostLayoutChanged(bool fromWindowResize);

    bool viewportEventFilter(QObject *watched, QEvent *event);
    TermWidgetImpl *currentImpl();

private:
    enum class Cli { Claude, Gemini, Codex, Unknown };
    TermWidget *currentTermWidget();
    static Cli detectCli(TermWidgetImpl *impl);
    static void sendCtrlKey(TermWidgetImpl *impl, int key);
    static void sendKey(TermWidgetImpl *impl, int key);
    void clearTerminalInput(TermWidgetImpl *impl);
    QString normalizeSelection(const QString &text) const;
    void positionComposeEditor();
    void applyCurrentTerminalOffset();
    int currentComposeOffset() const;
    void setPtyResizeSuspended(QTermWidget *impl, bool suspended);
    void clearAllPtyResizeSuspended();

    QWidget *m_container = nullptr;
    TabWidget *m_tabulator;
    QPlainTextEdit *m_editor = nullptr;
    QRect m_tabBaseline;
    bool m_active = false;
    bool m_rawMode = false;
    bool m_submitInProgress = false;
    QHash<QTermWidget *, bool> m_resizeSuspended;
};

#endif
