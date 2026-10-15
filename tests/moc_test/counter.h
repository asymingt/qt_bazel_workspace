#ifndef TESTS_MOC_TEST_COUNTER_H_
#define TESTS_MOC_TEST_COUNTER_H_

#include <QObject>

class Counter : public QObject {
    Q_OBJECT

public:
    explicit Counter(QObject *parent = nullptr) : QObject(parent), m_value(0) {}
    int value() const { return m_value; }

public slots:
    void setValue(int value) {
        if (value != m_value) {
            m_value = value;
            emit valueChanged(value);
        }
    }

signals:
    void valueChanged(int newValue);

private:
    int m_value;
};

#endif  // TESTS_MOC_TEST_COUNTER_H_
