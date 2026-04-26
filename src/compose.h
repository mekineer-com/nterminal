#ifndef COMPOSE_H
#define COMPOSE_H

#include <QObject>

class QPlainTextEdit;
class QAction;
class QGridLayout;
class QWidget;
class QEvent;
class TabWidget;
class TermWidgetImpl;
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

    TabWidget *m_tabulator;
    QPlainTextEdit *m_editor = nullptr;
    QAction *m_toggleAction = nullptr;
    bool m_active = false;
    bool m_rawMode = false;
    bool m_submitInProgress = false;
    bool m_heightInitialized = false;
};

#endif
