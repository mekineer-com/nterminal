#include "compose.h"

#include <QPlainTextEdit>
#include <QAction>
#include <QGridLayout>
#include <QShortcut>
#include <QTimer>
#include <QTextOption>
#include <QTextDocument>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QRegularExpression>
#include <QFile>
#include <QGuiApplication>
#include <QClipboard>
#include <cmath>
#include <algorithm>
#include <limits>

#include "tabwidget.h"
#include "termwidgetholder.h"
#include "termwidget.h"

namespace {

QString readProcCmdline(int pid)
{
    if (pid <= 0)
    {
        return QString();
    }

    QFile file(QStringLiteral("/proc/%1/cmdline").arg(pid));
    if (!file.open(QIODevice::ReadOnly))
    {
        return QString();
    }

    QByteArray raw = file.readAll();
    if (raw.isEmpty())
    {
        return QString();
    }

    raw.replace('\0', ' ');
    return QString::fromLocal8Bit(raw).trimmed();
}

} // namespace

ComposeInput::ComposeInput(QWidget *container, QGridLayout *layout, TabWidget *tabulator, QObject *parent)
    : QObject(parent),
      m_tabulator(tabulator)
{
    m_active = qEnvironmentVariableIsSet("NTERMINAL_COMPOSE")
        || QCoreApplication::applicationName().toLower().contains(QStringLiteral("nterminal"));
    if (!m_active)
    {
        return;
    }

    m_editor = new QPlainTextEdit(container);
    m_editor->setObjectName(QStringLiteral("composeInput"));
    m_editor->setFrameShape(QFrame::NoFrame);
    m_editor->setTabChangesFocus(false);
    m_editor->setUndoRedoEnabled(true);
    m_editor->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_editor->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_editor->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    m_editor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_editor->document()->setDocumentMargin(2);
    m_editor->setPlaceholderText(tr("Compose: Enter newline, Ctrl+Enter send, F6 raw mode"));
    m_editor->setStyleSheet(QStringLiteral("QPlainTextEdit#composeInput { border: 0; }"));

    layout->addWidget(m_editor, 1, 0);
    layout->setRowStretch(0, 1);
    layout->setRowStretch(1, 0);

    m_editor->viewport()->installEventFilter(this);

    connect(m_editor->document(), &QTextDocument::contentsChanged, this, [this]() {
        QTimer::singleShot(0, this, &ComposeInput::updateHeight);
    });

    auto *sendReturn = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return), m_editor);
    connect(sendReturn, &QShortcut::activated, this, &ComposeInput::send);
    auto *sendEnter = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Enter), m_editor);
    connect(sendEnter, &QShortcut::activated, this, &ComposeInput::send);

    updateHeight();
    setRawInputMode(false);
    m_heightInitialized = true;
}

TermWidgetImpl *ComposeInput::currentImpl()
{
    TermWidgetHolder *h = m_tabulator->terminalHolder();
    if (h == nullptr) return nullptr;
    TermWidget *t = h->currentTerminal();
    return t != nullptr ? t->impl() : nullptr;
}

TermWidget *ComposeInput::currentTermWidget()
{
    TermWidgetHolder *h = m_tabulator->terminalHolder();
    return h != nullptr ? h->currentTerminal() : nullptr;
}

void ComposeInput::updateHeight()
{
    if (m_editor == nullptr)
    {
        return;
    }

    bool ok = false;
    int maxLines = qEnvironmentVariableIntValue("NTERMINAL_COMPOSE_MAX_LINES", &ok);
    if (!ok || maxLines < 2)
    {
        maxLines = 12;
    }

    const qreal docHeight = m_editor->document()->size().height();
    int visualLines = std::max(1, static_cast<int>(std::ceil(docHeight)));
    visualLines = std::min(visualLines, maxLines);

    const QFontMetrics fm(m_editor->font());
    const int padding = 8;
    const int frame = m_editor->frameWidth() * 2;
    const int newHeight = frame + padding + (visualLines * fm.lineSpacing());

    if (m_heightInitialized)
    {
        if (TermWidgetImpl *impl = currentImpl())
            impl->setSuppressPtyResize(true);
    }

    m_editor->setFixedHeight(newHeight);

    if (m_heightInitialized && visualLines == 1)
    {
        // Compose returned to 1 line (submit/clear). Unsuppress and tell
        // the pty the correct size so the app renders correctly.
        QTimer::singleShot(50, this, [this]() {
            if (TermWidgetImpl *impl = currentImpl())
            {
                impl->setSuppressPtyResize(false);
                impl->sendCurrentSizeToPty();
            }
        });
    }
}

void ComposeInput::focusTerminal()
{
    if (TermWidgetImpl *impl = currentImpl())
    {
        impl->setFocus(Qt::OtherFocusReason);
    }
}

void ComposeInput::setRawInputMode(bool raw)
{
    if (m_editor == nullptr)
    {
        return;
    }

    m_rawMode = raw;
    m_editor->setVisible(!raw);

    if (raw)
    {
        focusTerminal();
    }
    else
    {
        updateHeight();
        m_editor->setFocus(Qt::OtherFocusReason);
    }
}

bool ComposeInput::viewportEventFilter(QObject *watched, QEvent *event)
{
    if (m_editor != nullptr && watched == m_editor->viewport()
        && event->type() == QEvent::MouseMove)
    {
        auto *me = static_cast<QMouseEvent*>(event);
        if (m_editor->isVisible() && (me->buttons() & Qt::LeftButton))
        {
            const QRect r = m_editor->viewport()->rect();
            if (!r.contains(me->position().toPoint()))
            {
                return true;
            }
        }
    }
    return false;
}

ComposeInput::Cli ComposeInput::detectCli(TermWidgetImpl *impl)
{
    if (impl == nullptr)
    {
        return Cli::Unknown;
    }

    const int foregroundPid = impl->getForegroundProcessId();
    const int shellPid = impl->getShellPID();

    const QStringList candidates = {
        readProcCmdline(foregroundPid),
        readProcCmdline(shellPid)
    };

    for (const QString &candidate : candidates)
    {
        if (candidate.isEmpty())
        {
            continue;
        }

        const QString cmd = candidate.toLower();
        if (cmd.contains(QStringLiteral("claude")))
        {
            return Cli::Claude;
        }
        if (cmd.contains(QStringLiteral("codex")))
        {
            return Cli::Codex;
        }
        if (cmd.contains(QStringLiteral("gemini")))
        {
            return Cli::Gemini;
        }
    }

    return Cli::Unknown;
}

void ComposeInput::sendCtrlKey(TermWidgetImpl *impl, int key)
{
    if (impl == nullptr) return;
    QKeyEvent press(QEvent::KeyPress, key, Qt::ControlModifier, QString());
    QKeyEvent release(QEvent::KeyRelease, key, Qt::ControlModifier, QString());
    impl->sendKeyEvent(&press);
    impl->sendKeyEvent(&release);
}

void ComposeInput::sendKey(TermWidgetImpl *impl, int key)
{
    if (impl == nullptr) return;
    QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier, QString());
    QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier, QString());
    impl->sendKeyEvent(&press);
    impl->sendKeyEvent(&release);
}

void ComposeInput::clearTerminalInput(TermWidgetImpl *impl)
{
    if (impl == nullptr) return;

    const Cli cli = detectCli(impl);

    if (cli == Cli::Claude)
    {
        impl->sendText(QStringLiteral("\x15"));
        return;
    }

    if (cli == Cli::Gemini)
    {
        constexpr int kDownPasses = 8;
        for (int i = 0; i < kDownPasses; ++i)
        {
            sendKey(impl, Qt::Key_Down);
        }
        impl->sendText(QString(QChar(0x05)));
        constexpr int kPasses = 8;
        for (int i = 0; i < kPasses; ++i)
        {
            impl->sendText(QString(QChar(0x15)));
            sendKey(impl, Qt::Key_Backspace);
        }
        return;
    }

    if (cli == Cli::Codex)
    {
        constexpr int kPasses = 8;
        for (int i = 0; i < kPasses; ++i)
        {
            sendCtrlKey(impl, Qt::Key_K);
            sendCtrlKey(impl, Qt::Key_U);
        }
        return;
    }

    constexpr int kPasses = 8;
    for (int i = 0; i < kPasses; ++i)
    {
        sendCtrlKey(impl, Qt::Key_K);
        sendCtrlKey(impl, Qt::Key_U);
    }
}

QString ComposeInput::normalizeSelection(const QString &text) const
{
    QString cleaned = text;
    cleaned.remove(QLatin1Char('\r'));

    static const QRegularExpression trailingWhitespace(QStringLiteral("[ \\t]+(?=\\n|$)"));
    cleaned.replace(trailingWhitespace, QString());

    while (cleaned.startsWith(QLatin1Char('\n')))
    {
        cleaned.remove(0, 1);
    }
    while (cleaned.endsWith(QLatin1Char('\n')))
    {
        cleaned.chop(1);
    }

    // Strip QTermWidget's soft-wrap indentation (2 leading spaces on
    // continuation lines) when first line is unindented.
    QStringList lines = cleaned.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    if (lines.size() > 1)
    {
        const QString &first = lines.at(0);
        const bool firstIndented = first.startsWith(QLatin1Char(' ')) || first.startsWith(QLatin1Char('\t'));

        int minContinuationIndent = std::numeric_limits<int>::max();
        bool sawContinuation = false;
        bool allContinuationHaveIndent = true;

        for (int i = 1; i < lines.size(); ++i)
        {
            const QString &line = lines.at(i);
            if (line.isEmpty()) continue;

            sawContinuation = true;
            int spaces = 0;
            while (spaces < line.size() && line.at(spaces) == QLatin1Char(' '))
            {
                ++spaces;
            }
            if (spaces == 0)
            {
                allContinuationHaveIndent = false;
                break;
            }
            minContinuationIndent = std::min(minContinuationIndent, spaces);
        }

        if (!firstIndented && sawContinuation && allContinuationHaveIndent && minContinuationIndent >= 2)
        {
            for (int i = 1; i < lines.size(); ++i)
            {
                QString &line = lines[i];
                int toRemove = 0;
                while (toRemove < 2 && toRemove < line.size() && line.at(toRemove) == QLatin1Char(' '))
                {
                    ++toRemove;
                }
                if (toRemove > 0)
                {
                    line.remove(0, toRemove);
                }
            }
            cleaned = lines.join(QLatin1Char('\n'));
        }
    }

    return cleaned;
}

void ComposeInput::send()
{
    if (m_editor == nullptr) return;
    if (m_submitInProgress) return;

    QString text = m_editor->toPlainText();
    if (text.trimmed().isEmpty()) return;

    TermWidgetImpl *impl = currentImpl();
    if (impl == nullptr) return;

    const Cli cli = detectCli(impl);
    // Ctrl+Enter inserts a newline before the shortcut fires; strip only the trailing one.
    if (text.endsWith(QStringLiteral("\r\n")))
    {
        text.chop(2);
    }
    else if (text.endsWith(QLatin1Char('\n')) || text.endsWith(QLatin1Char('\r')))
    {
        text.chop(1);
    }

    m_submitInProgress = true;
    clearTerminalInput(impl);
    auto finish = [this]() { m_submitInProgress = false; };
    auto findImpl = [this]() -> TermWidgetImpl* { return currentImpl(); };

    if (cli == Cli::Claude)
    {
        // 200ms between steps — Claude's async input handler needs settle time.
        QTimer::singleShot(200, this, [this, text, findImpl, finish]() {
            TermWidgetImpl *i = findImpl();
            if (i == nullptr) { finish(); return; }
            i->sendText(text);
            QTimer::singleShot(200, this, [this, findImpl, finish]() {
                TermWidgetImpl *i2 = findImpl();
                if (i2 != nullptr) i2->sendText(QString(QLatin1Char('\r')));
                finish();
            });
        });
    }
    else if (cli == Cli::Gemini)
    {
        // Leading '?' fires the help menu so text is treated as literal (dc11ca6).
        impl->sendText(QStringLiteral("?"));
        QTimer::singleShot(100, this, [this, text, findImpl, finish]() {
            TermWidgetImpl *i = findImpl();
            if (i == nullptr) { finish(); return; }
            i->sendText(text);
            QTimer::singleShot(200, this, [this, findImpl, finish]() {
                TermWidgetImpl *i2 = findImpl();
                if (i2 != nullptr) i2->sendText(QString(QLatin1Char('\r')));
                finish();
            });
        });
    }
    else if (cli == Cli::Codex)
    {
        impl->sendText(text);
        QTimer::singleShot(100, this, [this, findImpl, finish]() {
            TermWidgetImpl *i = findImpl();
            if (i != nullptr) sendKey(i, Qt::Key_Return);
            finish();
        });
    }
    else
    {
        // sendText("\r") not sendKey(Return) — avoids extra Enter in readline (0c1201c).
        impl->sendText(text);
        QTimer::singleShot(100, this, [this, findImpl, finish]() {
            TermWidgetImpl *i = findImpl();
            if (i != nullptr) i->sendText(QString(QLatin1Char('\r')));
            finish();
        });
    }

    m_editor->clear();
    updateHeight();

    if (m_rawMode)
    {
        focusTerminal();
    }
    else
    {
        m_editor->setFocus(Qt::OtherFocusReason);
    }
}

void ComposeInput::transferToTerminal()
{
    if (m_editor == nullptr) return;

    QString text = m_editor->toPlainText();
    if (text.isEmpty())
    {
        focusTerminal();
        return;
    }

    if (text.endsWith(QStringLiteral("\r\n")))
    {
        text.chop(2);
    }
    else if (text.endsWith(QLatin1Char('\n')) || text.endsWith(QLatin1Char('\r')))
    {
        text.chop(1);
    }

    TermWidgetImpl *impl = currentImpl();
    if (impl == nullptr) return;

    clearTerminalInput(impl);
    const Cli cli = detectCli(impl);

    if (cli == Cli::Claude)
    {
        QTimer::singleShot(100, this, [this, text]() {
            TermWidgetImpl *i = currentImpl();
            if (i == nullptr) return;
            i->sendText(QStringLiteral("\x1b[200~") + text + QStringLiteral("\x1b[201~"));
            focusTerminal();
        });
    }
    else if (cli == Cli::Gemini)
    {
        impl->sendText(QStringLiteral("?"));
        QTimer::singleShot(100, this, [this, text]() {
            TermWidgetImpl *i = currentImpl();
            if (i == nullptr) return;
            i->sendText(text);
            focusTerminal();
        });
    }
    else
    {
        impl->sendText(text);
        focusTerminal();
    }
}

void ComposeInput::transferFromTerminal()
{
    if (m_editor == nullptr) return;

    TermWidget *term = currentTermWidget();
    if (term == nullptr) return;
    TermWidgetImpl *impl = term->impl();
    if (impl == nullptr) return;

    QString source = impl->selectedText(true);
    if (source.isEmpty())
    {
        source = term->lastSelectedText();
    }

    const QString normalized = normalizeSelection(source);
    if (normalized.isEmpty())
    {
        m_editor->setFocus(Qt::OtherFocusReason);
        return;
    }

    setRawInputMode(false);
    m_editor->setFocus(Qt::OtherFocusReason);
    m_editor->insertPlainText(normalized);
    updateHeight();
    term->clearLastSelectedText();
}
