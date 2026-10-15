#include <QRegularExpression>
#include <QString>
#include <cassert>

int main() {
    QRegularExpression re(QStringLiteral("^Qt\\s+(\\d+)\\.(\\d+)\\.(\\d+)$"));
    assert(re.isValid());
    QRegularExpressionMatch match = re.match(QStringLiteral("Qt 6.10.2"));
    assert(match.hasMatch());
    assert(match.captured(1) == QStringLiteral("6"));
    assert(match.captured(2) == QStringLiteral("10"));
    assert(match.captured(3) == QStringLiteral("2"));
    return 0;
}
