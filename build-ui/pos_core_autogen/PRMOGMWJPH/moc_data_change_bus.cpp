/****************************************************************************
** Meta object code from reading C++ file 'data_change_bus.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/core/data_change_bus.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'data_change_bus.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.11.1. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN3pos13DataChangeBusE_t {};
} // unnamed namespace

template <> constexpr inline auto pos::DataChangeBus::qt_create_metaobjectdata<qt_meta_tag_ZN3pos13DataChangeBusE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "pos::DataChangeBus",
        "inventoryChanged",
        "",
        "salesChanged",
        "purchasesChanged",
        "customersChanged",
        "suppliersChanged",
        "cashChanged"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'inventoryChanged'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'salesChanged'
        QtMocHelpers::SignalData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'purchasesChanged'
        QtMocHelpers::SignalData<void()>(4, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'customersChanged'
        QtMocHelpers::SignalData<void()>(5, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'suppliersChanged'
        QtMocHelpers::SignalData<void()>(6, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'cashChanged'
        QtMocHelpers::SignalData<void()>(7, 2, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<DataChangeBus, qt_meta_tag_ZN3pos13DataChangeBusE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject pos::DataChangeBus::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN3pos13DataChangeBusE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN3pos13DataChangeBusE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN3pos13DataChangeBusE_t>.metaTypes,
    nullptr
} };

void pos::DataChangeBus::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<DataChangeBus *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->inventoryChanged(); break;
        case 1: _t->salesChanged(); break;
        case 2: _t->purchasesChanged(); break;
        case 3: _t->customersChanged(); break;
        case 4: _t->suppliersChanged(); break;
        case 5: _t->cashChanged(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (DataChangeBus::*)()>(_a, &DataChangeBus::inventoryChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (DataChangeBus::*)()>(_a, &DataChangeBus::salesChanged, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (DataChangeBus::*)()>(_a, &DataChangeBus::purchasesChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (DataChangeBus::*)()>(_a, &DataChangeBus::customersChanged, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (DataChangeBus::*)()>(_a, &DataChangeBus::suppliersChanged, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (DataChangeBus::*)()>(_a, &DataChangeBus::cashChanged, 5))
            return;
    }
}

const QMetaObject *pos::DataChangeBus::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *pos::DataChangeBus::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN3pos13DataChangeBusE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int pos::DataChangeBus::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 6)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 6;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 6)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 6;
    }
    return _id;
}

// SIGNAL 0
void pos::DataChangeBus::inventoryChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void pos::DataChangeBus::salesChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void pos::DataChangeBus::purchasesChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void pos::DataChangeBus::customersChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void pos::DataChangeBus::suppliersChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void pos::DataChangeBus::cashChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 5, nullptr);
}
QT_WARNING_POP
