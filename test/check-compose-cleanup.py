#!/usr/bin/env python3
"""Check the actual compose menu action and Undo without a terminal/session."""
import os
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'src/compose.cpp').read_text()
menu = re.search(r'    m_editor->setContextMenuPolicy.*?^    \}\);', source, re.M | re.S)
normalizer = re.search(r'^QString ComposeInput::normalizeSelection.*?^\}', source, re.M | re.S)
assert menu and normalizer
fixture = r'''
#include <QtWidgets>
#include <limits>
#include "qterminalutils.h"
class ComposeInput : public QObject {
public:
    QPlainTextEdit *m_editor = new QPlainTextEdit;
    ComposeInput() {
'''+menu.group()+r'''
    }
    ~ComposeInput() { delete m_editor; }
    QString normalizeSelection(const QString &) const;
    void clean(bool enabled = true) {
        QTimer::singleShot(0, [enabled]() {
            auto *menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
            Q_ASSERT(menu);
            QAction *cleanup = nullptr;
            for(auto *action : menu->actions())
                if(action->text() == "Clean up spacing") cleanup = action;
            Q_ASSERT(cleanup && cleanup->isEnabled() == enabled);
            if(enabled) cleanup->trigger();
            menu->close();
        });
        emit m_editor->customContextMenuRequested(QPoint(5, 5));
    }
};
'''+normalizer.group()+r'''
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    ComposeInput input;
    auto *editor = input.m_editor;
    editor->show();
    const QString original = "first  \n  second \t\n\n";
    editor->setPlainText(original);
    input.clean();
    Q_ASSERT(editor->toPlainText() == "first\nsecond");
    editor->undo();
    Q_ASSERT(editor->toPlainText() == original);
    const QString selected = "keep  \nfirst  \n  second \t\nkeep  ";
    editor->setPlainText(selected);
    QTextCursor cursor = editor->textCursor();
    cursor.setPosition(selected.indexOf("first"));
    cursor.setPosition(selected.lastIndexOf("\nkeep"), QTextCursor::KeepAnchor);
    editor->setTextCursor(cursor);
    input.clean();
    Q_ASSERT(editor->toPlainText() == "keep  \nfirst\nsecond\nkeep  ");
    editor->undo();
    Q_ASSERT(editor->toPlainText() == selected);
    editor->setReadOnly(true);
    input.clean(false);
    Q_ASSERT(editor->toPlainText() == selected);
}
'''
with tempfile.TemporaryDirectory(prefix='nterminal-cleanup-') as directory:
    work = Path(directory)
    (work / 'check.cpp').write_text(fixture)
    flags = subprocess.check_output(['pkg-config', '--cflags', '--libs', 'Qt6Widgets'], text=True).split()
    subprocess.run(['c++', '-std=c++17', str(work / 'check.cpp'), str(root / 'src/qterminalutils.cpp'),
                    '-I'+str(root / 'src'), '-o', str(work / 'check'), *flags], check=True)
    subprocess.run([str(work / 'check')], env=dict(os.environ, QT_QPA_PLATFORM='offscreen',
                   HOME=directory, XDG_CONFIG_HOME=directory), check=True, timeout=10)
print('PASS: selection/whole editor, one-step Undo, and read-only menu state')
