//  Natron
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * Created by Alexandre GAUTHIER-FOICHAT on 6/1/2012.
 * contact: immarespond at gmail dot com
 *
 */

// from <https://docs.python.org/3/c-api/intro.html#include-files>:
// "Since Python may define some pre-processor definitions which affect the standard headers on some systems, you must include Python.h before any standard headers are included."
#include <Python.h>

#include "SpinBox.h"

#include <cfloat>
#include <cmath>
CLANG_DIAG_OFF(unused-private-field)
// /opt/local/include/QtGui/qmime.h:119:10: warning: private field 'type' is not used [-Wunused-private-field]
#include <QtGui/QWheelEvent>
CLANG_DIAG_ON(unused-private-field)
#include <QtGui/QDoubleValidator>
#include <QtGui/QIntValidator>
#include <QStyle> // in QtGui on Qt4, in QtWidgets on Qt5
#include <QApplication>
#include <QDebug>

#include "Engine/Variant.h"
#include "Engine/Settings.h"
#include "Engine/AppManager.h"
#include "Global/Macros.h"
#include "Gui/GuiMacros.h"


struct SpinBoxPrivate
{
    SpinBox::SpinBoxTypeEnum type;
    
    /*the precision represents the number of digits after the decimal point.
     For the 'g' and 'G' formats, the precision represents the maximum number
     of significant digits (trailing zeroes are omitted)*/
    int decimals; // for the double spinbox only
    double increment;
    int currentDelta; // accumulates the deltas from wheelevents
    Variant mini,maxi;
    QDoubleValidator* doubleValidator;
    QIntValidator* intValidator;
    double valueWhenEnteringFocus;
    bool hasChangedSinceLastValidation;
    double valueAfterLastValidation;
    bool valueInitialized; //< false when setValue has never been called yet.
    
    SpinBoxPrivate(SpinBox::SpinBoxTypeEnum type)
    : type(type)
    , decimals(2)
    , increment(1.0)
    , currentDelta(0)
    , mini()
    , maxi()
    , doubleValidator(0)
    , intValidator(0)
    , valueWhenEnteringFocus(0)
    , hasChangedSinceLastValidation(false)
    , valueAfterLastValidation(0)
    , valueInitialized(false)
    {
    }
    
    QString setNum(double cur);
};

SpinBox::SpinBox(QWidget* parent,
                 SpinBoxTypeEnum type)
: LineEdit(parent)
, animation(0)
, dirty(false)
, _imp( new SpinBoxPrivate(type) )
{
    QObject::connect( this, SIGNAL( returnPressed() ), this, SLOT( interpretReturn() ) );
    setValue(0);
    setMaximumWidth(50);
    setMinimumWidth(35);
    setFocusPolicy(Qt::WheelFocus); // mouse wheel gives focus too - see also SpinBox::focusInEvent()
    decimals(_imp->decimals);

    setType(type);
}

SpinBox::~SpinBox()
{
    switch (_imp->type) {
        case eSpinBoxTypeDouble:
            delete _imp->doubleValidator;
            break;
        case eSpinBoxTypeInt:
            delete _imp->intValidator;
            break;
    }
}

void
SpinBox::setType(SpinBoxTypeEnum type)
{
    _imp->type = type;
    if (_imp->doubleValidator) {
        delete _imp->doubleValidator;
        _imp->doubleValidator = 0;
    }
    if (_imp->intValidator) {
        delete _imp->intValidator;
        _imp->intValidator = 0;
    }
    switch (_imp->type) {
        case eSpinBoxTypeDouble:
            _imp->mini.setValue<double>(-DBL_MAX);
            _imp->maxi.setValue<double>(DBL_MAX);
            _imp->doubleValidator = new QDoubleValidator;
            _imp->doubleValidator->setTop(DBL_MAX);
            _imp->doubleValidator->setBottom(-DBL_MAX);
            setValue_internal(value(),true);
            break;
        case eSpinBoxTypeInt:
            _imp->intValidator = new QIntValidator;
            _imp->mini.setValue<int>(INT_MIN);
            _imp->maxi.setValue<int>(INT_MAX);
            _imp->intValidator->setTop(INT_MAX);
            _imp->intValidator->setBottom(INT_MIN);
            setValue_internal((int)std::floor(value() + 0.5),true);

            break;
    }
}

void
SpinBox::setValue_internal(double d,
                           bool reformat)
{
    if ( ( d == text().toDouble() ) && !reformat && _imp->valueInitialized ) {
        // the value is already OK
        return;
    }
    _imp->valueWhenEnteringFocus = d;
    int pos = cursorPosition();
    QString str;
    switch (_imp->type) {
        case eSpinBoxTypeDouble: {
            str.setNum(d, 'f', _imp->decimals);
            double toDouble = str.toDouble();
            if (d != toDouble) {
                str.setNum(d, 'g', 15);
            }
        }   break;
        case eSpinBoxTypeInt:
            str.setNum( (int)d );
            break;
    }
    assert( !str.isEmpty() );
    
    ///Remove trailing 0s by hand...
    int decimalPtPos = str.indexOf('.');
    if (decimalPtPos != -1) {
        int i = str.size() - 1;
        while (i > decimalPtPos && str.at(i) == '0') {
            --i;
        }
        ///let 1 trailing 0
        if (i < str.size() - 1) {
            ++i;
        }
        str = str.left(i + 1);
    }
    
    // The following removes leading zeroes, but this is not necessary
    /*
     int i = 0;
     bool skipFirst = false;
     if (str.at(0) == '-') {
     skipFirst = true;
     ++i;
     }
     while (i < str.size() && i == '0') {
     ++i;
     }
     if (i > int(skipFirst)) {
     str = str.remove(int(skipFirst), i - int(skipFirst));
     }
     */
    setText(str, pos);
    _imp->valueInitialized = true;
}

void
SpinBox::setText(const QString &str,
                 int cursorPos)
{
    QLineEdit::setText(str);
    
    //qDebug() << "text:" << str << " cursorpos: " << cursorPos;
    setCursorPosition(cursorPos);
    _imp->hasChangedSinceLastValidation = false;
    _imp->valueAfterLastValidation = value();
}

void
SpinBox::setValue(double d)
{
    setValue_internal(d, false);
}

void
SpinBox::setValue(int d)
{
    setValue( (double)d );
}

void
SpinBox::interpretReturn()
{
    if ( validateText() ) {
        //setValue_internal(text().toDouble(), true, true); // force a reformat
        Q_EMIT valueChanged( value() );
    }
}

/*
 void
 SpinBox::mousePressEvent(QMouseEvent* e)
 {
 //setCursorPosition(cursorPositionAt(e->pos())); // LineEdit::mousePressEvent(e) does this already
 LineEdit::mousePressEvent(e);
 }
 */

QString
SpinBoxPrivate::setNum(double cur)
{
    switch (type) {
        case SpinBox::eSpinBoxTypeInt:
            
            return QString().setNum( (int)cur );
        case SpinBox::eSpinBoxTypeDouble:
        default:
            
            return QString().setNum(cur,'f',decimals);
    }
}

// common function for wheel up/down and key up/down
// delta is in wheel units: a delta of 120 means to increment by 1
void
SpinBox::increment(int delta,
                   int shift) // shift = 1 means to increment * 10, shift = -1 means / 10
{
    bool ok;
    QString str = text();
    //qDebug() << "increment from " << str;
    const double oldVal = str.toDouble(&ok);
    
    if (!ok) {
        // Not a valid double value, don't do anything
        return;
    }
    
    bool useCursorPositionIncr = appPTR->getCurrentSettings()->useCursorPositionIncrements();
    
    // First, treat the standard case: use the Knob increment
    if (!useCursorPositionIncr) {
        double val = oldVal;
        _imp->currentDelta += delta;
        double inc = std::pow(10., shift) * _imp->currentDelta * _imp->increment / 120.;
        double maxiD = 0.;
        double miniD = 0.;
        switch (_imp->type) {
            case eSpinBoxTypeDouble: {
                maxiD = _imp->maxi.toDouble();
                miniD = _imp->mini.toDouble();
                val += inc;
                _imp->currentDelta = 0;
                break;
            }
            case eSpinBoxTypeInt: {
                maxiD = _imp->maxi.toInt();
                miniD = _imp->mini.toInt();
                val += (int)inc;     // round towards zero
                // Update the current delta, which contains the accumulated error
                _imp->currentDelta -= ( (int)inc ) * 120. / _imp->increment;
                assert(std::abs(_imp->currentDelta) < 120);
                break;
            }
        }
        val = std::max( miniD, std::min(val, maxiD) );
        if (val != oldVal) {
            setValue(val);
            Q_EMIT valueChanged(val);
        }
        
        return;
    }
    
    // From here on, we treat the positin-based increment.
    
    if ( (str.indexOf('e') != -1) || (str.indexOf('E') != -1) ) {
        // Sorry, we don't handle numbers with an exponent, although these are valid doubles
        return;
    }
    
    _imp->currentDelta += delta;
    int inc_int = _imp->currentDelta / 120; // the number of integert increments
    // Update the current delta, which contains the accumulated error
    _imp->currentDelta -= inc_int * 120;
    
    if (inc_int == 0) {
        // Nothing is changed, just return
        return;
    }
    
    // Within the value, we modify:
    // - if there is no selection, the first digit right after the cursor (or if it is an int and the cursor is at the end, the last digit)
    // - if there is a selection, the first digit after the start of the selection
    int len = str.size(); // used for chopping spurious characters
    if (len <= 0) {
        return; // should never happen
    }
    // The position in str of the digit to modify in str() (may be equal to str.size())
    int pos = ( hasSelectedText() ? selectionStart() : cursorPosition() );
    //if (pos == len) { // select the last character?
    //    pos = len - 1;
    //}
    // The position of the decimal dot
    int dot = str.indexOf('.');
    if (dot == -1) {
        dot = str.size();
    }
    
    // Now, chop trailing and leading whitespace (and update len, pos and dot)
    
    // Leading whitespace
    while ( len > 0 && str[0].isSpace() ) {
        str.remove(0, 1);
        --len;
        if (pos > 0) {
            --pos;
        }
        --dot;
        assert(dot >= 0);
        assert(len > 0);
    }
    // Trailing whitespace
    while ( len > 0 && str[len - 1].isSpace() ) {
        str.remove(len - 1, 1);
        --len;
        if (pos > len) {
            --pos;
        }
        if (dot > len) {
            --dot;
        }
        assert(len > 0);
    }
    assert( oldVal == str.toDouble() ); // check that the value hasn't changed due to whitespace manipulation
    
    // On int types, there should not be any dot
    if ( (_imp->type == eSpinBoxTypeInt) && (len > dot) ) {
        // Remove anything after the dot, including the dot
        str.resize(dot);
        len = dot;
    }
    
    // Adjust pos so that it doesn't point to a dot or a sign
    assert( 0 <= pos && pos <= str.size() );
    while ( pos < str.size() &&
           (pos == dot || str[pos] == '+' || str[pos] == '-') ) {
        ++pos;
    }
    assert(len >= pos);
    
    // Set the shift (may have to be done twice due to the dot)
    pos -= shift;
    if (pos == dot) {
        pos -= shift;
    }
    
    // Now, add leading and trailing zeroes so that pos is a valid digit position
    // (beware of the sign!)
    // Trailing zeroes:
    // (No trailing zeroes on int, of course)
    assert( len == str.size() );
    if ( (_imp->type == eSpinBoxTypeInt) && (pos >= len) ) {
        // If this is an int and we are beyond the last position, change the last digit
        pos = len - 1;
        // also reset the shift if it was negative
        if (shift < 0) {
            shift = 0;
        }
    }
    while ( pos >= str.size() ) {
        assert(_imp->type == eSpinBoxTypeDouble);
        // Add trailing zero, maybe preceded by a dot
        if (pos == dot) {
            str.append('.');
            ++pos; // increment pos, because we just added a '.', and next iteration will add a '0'
            ++len;
        } else {
            assert(pos > dot);
            str.append('0');
            ++len;
        }
        assert( pos >= (str.size() - 1) );
    }
    // Leading zeroes:
    bool hasSign = (str[0] == '-' || str[0] == '+');
    while ( pos < 0 || ( pos == 0 && (str[0] == '-' || str[0] == '+') ) ) {
        // Add leading zero
        str.insert(hasSign ? 1 : 0, '0');
        ++pos;
        ++dot;
        ++len;
    }
    assert( len == str.size() );
    assert( 0 <= pos && pos < str.size() && str[pos].isDigit() );
    
    QString noDotStr = str;
    int noDotLen = len;
    if (dot != len) {
        // Remove the dot
        noDotStr.remove(dot, 1);
        --noDotLen;
    }
    assert( (_imp->type == eSpinBoxTypeInt && noDotLen == dot) || noDotLen >= dot );
    double val = oldVal; // The value, as a double
    if (noDotLen > 16 && 16 >= dot) {
        // don't handle more than 16 significant digits (this causes over/underflows in the following)
        assert(noDotLen == noDotStr.size());
        noDotLen = 16;
        noDotStr.resize(noDotLen);
    }
    qlonglong llval = noDotStr.toLongLong(&ok); // The value, as a long long int
    if (!ok) {
        // Not a valid long long value, don't do anything
        return;
    }
    int llpowerOfTen = dot - noDotLen; // llval must be post-multiplied by this power of ten
    assert(llpowerOfTen <= 0);
    // check that val and llval*10^llPowerOfTen are close enough (relative error should be less than 1e-8)
    assert(std::abs(val * std::pow(10.,-llpowerOfTen) - llval) / std::max(qlonglong(1),std::abs(llval)) < 1e-8);
    
    
    // If pos is at the end
    if ( pos == str.size() ) {
        switch (_imp->type) {
            case eSpinBoxTypeDouble:
                if ( dot == str.size() ) {
                    str += ".0";
                    len += 2;
                    ++pos;
                } else {
                    str += "0";
                    ++len;
                }
                break;
            case eSpinBoxTypeInt:
                // take the character before
                --pos;
                break;
        }
    }
    
    // Compute the full value of the increment
    assert( len == str.size() );
    assert(pos != dot);
    assert( 0 <= pos && pos < len && str[pos].isDigit() );
    
    int powerOfTen = dot - pos - (pos < dot); // the power of ten
    assert( (_imp->type == eSpinBoxTypeDouble) || ( powerOfTen >= 0 && dot == str.size() ) );

    if (powerOfTen - llpowerOfTen > 16) {
        // too many digits to handle, don't do anything
        // (may overflow when adjusting llval)
        return;
    }

    double inc = inc_int * std::pow(10., (double)powerOfTen);
    
    // Check that we are within the authorized range
    double maxiD,miniD;
    switch (_imp->type) {
        case eSpinBoxTypeInt:
            maxiD = _imp->maxi.toInt();
            miniD = _imp->mini.toInt();
            break;
        case eSpinBoxTypeDouble:
        default:
            maxiD = _imp->maxi.toDouble();
            miniD = _imp->mini.toDouble();
            break;
    }
    val += inc;
    if ( (val < miniD) || (maxiD < val) ) {
        // out of the authorized range, don't do anything
        return;
    }

    // Adjust llval so that the increment becomes an int, and avoid rounding errors
    if (powerOfTen >= llpowerOfTen) {
        llval += inc_int * std::pow(10., powerOfTen - llpowerOfTen);
    } else {
        llval *= std::pow(10., llpowerOfTen - powerOfTen);
        llpowerOfTen -= llpowerOfTen - powerOfTen;
        llval += inc_int;
    }
    // check that val and llval*10^llPowerOfTen are still close enough (relative error should be less than 1e-8)
    assert(std::abs(val * std::pow(10.,-llpowerOfTen) - llval) / std::max(qlonglong(1),std::abs(llval)) < 1e-8);

    QString newStr;
    newStr.setNum(llval);
    bool newStrHasSign = newStr[0] == '+' || newStr[0] == '-';
    // the position of the decimal dot
    int newDot = newStr.size() + llpowerOfTen;
    // add leading zeroes if newDot is not a valid position (beware of sign!)
    while ( newDot <= int(newStrHasSign) ) {
        newStr.insert(int(newStrHasSign), '0');
        ++newDot;
    }
    assert( 0 <= newDot && newDot <= newStr.size() );
    assert( newDot == newStr.size() || newStr[newDot].isDigit() );
    if ( newDot != newStr.size() ) {
        assert(_imp->type == eSpinBoxTypeDouble);
        newStr.insert(newDot, '.');
    }
    // Check that the backed string is close to the wanted value (relative error should be less than 1e-8)
    assert( (newStr.toDouble() - val) / std::max(1e-8,std::abs(val)) < 1e-8 );
    // The new cursor position
    int newPos = newDot + (pos - dot);
    // Remove the shift (may have to be done twice due to the dot)
    newPos += shift;
    if (newPos == newDot) {
        // adjust newPos
        newPos += shift;
    }
    
    assert( 0 <= newDot && newDot <= newStr.size() );
    
    // Now, add leading and trailing zeroes so that newPos is a valid digit position
    // (beware of the sign!)
    // Trailing zeroes:
    while ( newPos >= newStr.size() ) {
        assert(_imp->type == eSpinBoxTypeDouble);
        // Add trailing zero, maybe preceded by a dot
        if (newPos == newDot) {
            newStr.append('.');
        } else {
            assert(newPos > newDot);
            newStr.append('0');
        }
        assert( newPos >= (newStr.size() - 1) );
    }
    // Leading zeroes:
    bool newHasSign = (newStr[0] == '-' || newStr[0] == '+');
    while ( newPos < 0 || ( newPos == 0 && (newStr[0] == '-' || newStr[0] == '+') ) ) {
        // add leading zero
        newStr.insert(newHasSign ? 1 : 0, '0');
        ++newPos;
        ++newDot;
    }
    assert( 0 <= newPos && newPos < newStr.size() && newStr[newPos].isDigit() );
    
    // Set the text and cursor position
    //qDebug() << "increment setting text to " << newStr;
    setText(newStr, newPos);
    // Set the selection
    assert( newPos + 1 <= newStr.size() );
    setSelection(newPos + 1, -1);
    Q_EMIT valueChanged( value() );
} // increment

void
SpinBox::wheelEvent(QWheelEvent* e)
{
    if ( (e->orientation() != Qt::Vertical) ||
        ( e->delta() == 0) ||
        !isEnabled() ||
        isReadOnly() ||
        !hasFocus() ) { // wheel is only effective when the widget has focus (click it first, then wheel)
        return;
    }
    int delta = e->delta();
    int shift = 0;
    if ( modCASIsShift(e) ) {
        shift = 1;
    }
    if ( modCASIsControl(e) ) {
        shift = -1;
    }
    increment(delta, shift);
}

void
SpinBox::focusInEvent(QFocusEvent* e)
{
    //qDebug() << "focusin";
    
    // don't give focus if this is a wheelEvent
    if (e->reason() == Qt::MouseFocusReason) {
        Qt::MouseButtons buttons = QApplication::mouseButtons();
        if ( !(buttons & Qt::LeftButton) && !(buttons & Qt::RightButton)) {
            this->clearFocus();
            
            return;
        }
    }
    _imp->valueWhenEnteringFocus = text().toDouble();
    LineEdit::focusInEvent(e);
}

void
SpinBox::focusOutEvent(QFocusEvent* e)
{
    //qDebug() << "focusout";
    double newValue = text().toDouble();
    
    if (newValue != _imp->valueWhenEnteringFocus) {
        if ( validateText() ) {
            //setValue_internal(text().toDouble(), true, true); // force a reformat
            Q_EMIT valueChanged( value() );
        }
    }
    LineEdit::focusOutEvent(e);
}

void
SpinBox::keyPressEvent(QKeyEvent* e)
{
    //qDebug() << "keypress";
    if ( isEnabled() && !isReadOnly() ) {
        if ( (e->key() == Qt::Key_Up) || (e->key() == Qt::Key_Down) ) {
            int delta = (e->key() == Qt::Key_Up) ? 120 : -120;
            int shift = 0;
            if ( modCASIsShift(e) ) {
                shift = 1;
            }
            if ( modCASIsControl(e) ) {
                shift = -1;
            }
            increment(delta, shift);
        } else {
            _imp->hasChangedSinceLastValidation = true;
            QLineEdit::keyPressEvent(e);
        }
    } else {
        QLineEdit::keyPressEvent(e);
    }
}

bool
SpinBox::validateText()
{
    if (!_imp->hasChangedSinceLastValidation) {
        return true;
    }
    
    double maxiD,miniD;
    switch (_imp->type) {
        case eSpinBoxTypeInt:
            maxiD = _imp->maxi.toInt();
            miniD = _imp->mini.toInt();
            break;
        case eSpinBoxTypeDouble:
        default:
            maxiD = _imp->maxi.toDouble();
            miniD = _imp->mini.toDouble();
            break;
    }
    
    switch (_imp->type) {
        case eSpinBoxTypeDouble: {
            QString txt = text();
            int tmp;
            QValidator::State st = _imp->doubleValidator->validate(txt,tmp);
            double val = txt.toDouble();
            if ( (st == QValidator::Invalid) ||
                (val < miniD) ||
                (val > maxiD) ) {
                setValue_internal(_imp->valueAfterLastValidation, true);
                
                return false;
            } else {
                setValue_internal(val, false);
                
                return true;
            }
            break;
        }
        case eSpinBoxTypeInt: {
            QString txt = text();
            int tmp;
            QValidator::State st = _imp->intValidator->validate(txt,tmp);
            int val = txt.toInt();
            if ( ( st == QValidator::Invalid) || ( val < miniD) || ( val > maxiD) ) {
                setValue_internal(_imp->valueAfterLastValidation, true);
                
                return false;
            } else {
                setValue_internal(val, false);
                
                return true;
            }
            break;
        }
    }
    
    return false;
} // validateText

void
SpinBox::decimals(int d)
{
    _imp->decimals = d;
}

void
SpinBox::setMaximum(double t)
{
    switch (_imp->type) {
        case eSpinBoxTypeDouble:
            _imp->maxi.setValue<double>(t);
            _imp->doubleValidator->setTop(t);
            break;
        case eSpinBoxTypeInt:
            _imp->maxi.setValue<int>( (int)t );
            _imp->intValidator->setTop(t);
            break;
    }
}

void
SpinBox::setMinimum(double b)
{
    switch (_imp->type) {
        case eSpinBoxTypeDouble:
            _imp->mini.setValue<double>(b);
            _imp->doubleValidator->setBottom(b);
            break;
        case eSpinBoxTypeInt:
            _imp->mini.setValue<int>(b);
            _imp->intValidator->setBottom(b);
            break;
    }
}

void
SpinBox::setIncrement(double d)
{
#ifdef OLD_SPINBOX_INCREMENT
    _imp->increment = d;
#else
    (void)d;
#endif
}

void
SpinBox::setAnimation(int i)
{
    animation = i;
    style()->unpolish(this);
    style()->polish(this);
    repaint();
}

void
SpinBox::setDirty(bool d)
{
    dirty = d;
    style()->unpolish(this);
    style()->polish(this);
    repaint();
}

QMenu*
SpinBox::getRightClickMenu()
{
    return createStandardContextMenu();
}

