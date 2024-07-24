#ifndef CMDHISTORYBUF_H
#define CMDHISTORYBUF_H

#include <QString>
#include <QList>
#include <QDebug>

#define HISTORY_BUF_SIZE 5

class CommandHistoryBuf {
public:
    CommandHistoryBuf(size_t size) : bufferSize(size) {
        resetPosition();
    }

    void insert(const QString& cmd) {
        if ( full() ) {
            buffer.takeFirst();
        }

        buffer.append(cmd);
        resetPosition();
    }

    QString getPreCommand() {
        QString sCmd = "";
        if ( empty() ) { return sCmd; }

        nIndex--;
        sCmd = buffer.value(nIndex);
        if ( nIndex < 0 ) {
            resetPosition();
        }

        return sCmd;
    }

    QString getNextCommand() {
        QString sCmd = "";
        if ( empty() ) { return sCmd; }

        nIndex++;
        if ( nIndex >= buffer.size() ) {
            nIndex = 0;
        }
        sCmd = buffer.value(nIndex);

        return sCmd;
    }

    void resetPosition() { nIndex = buffer.size(); }
    bool full() { return bufferSize == buffer.size(); }
    bool empty() { return buffer.size() == 0; }

private:
    QList<QString> buffer;
    size_t bufferSize;
    QList<QString>::iterator it;
    int nIndex = -1;
};

#endif // CMDHISTORYBUF_H
