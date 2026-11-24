#pragma once

#include <QApplication>
#include <QComboBox>
#include <QFrame>
#include <QPainter>
#include <QPushButton>
#include <QVariantAnimation>
#include <QStyledItemDelegate>


namespace ConnectionVisuals {

enum class Glyph { Power, Settings, Plus, Refresh, Globe, Check };

inline void drawGlyph(QPainter &p, Glyph glyph, QRectF box, QColor color) {
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.translate(box.topLeft());
    p.scale(box.width()/24, box.height()/24);
    p.setPen(QPen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    switch (glyph) {
    case Glyph::Power:
        p.drawLine(QPointF(12,3),QPointF(12,12));
        p.drawArc(QRectF(3,4,18,17),130*16,280*16);
        break;
    case Glyph::Settings:
        p.drawLine(3,7,7,7); p.drawEllipse(QPointF(10,7),2.5,2.5); p.drawLine(13,7,21,7);
        p.drawLine(3,17,12,17); p.drawEllipse(QPointF(15,17),2.5,2.5); p.drawLine(18,17,21,17);
        break;
    case Glyph::Plus:
        p.drawLine(12,5,12,19); p.drawLine(5,12,19,12); break;
    case Glyph::Refresh:
        p.drawArc(QRectF(4,4,16,16),50*16,277*16);
        p.drawPolyline(QPolygonF{QPointF(19,3),QPointF(19,8),QPointF(14,8)}); break;
    case Glyph::Globe:
        p.drawEllipse(QRectF(3,3,18,18)); p.drawEllipse(QRectF(8,3,8,18)); p.drawLine(4,12,20,12); break;
    case Glyph::Check:
        p.drawPolyline(QPolygonF{QPointF(5,12),QPointF(10,17),QPointF(19,7)}); break;
    }
    p.restore();
}

inline QIcon glyphIcon(Glyph glyph,QColor ink) {
    QIcon icon;
    for (int side : {16,20,24,32,48}) {
        QPixmap pix(side,side); pix.fill(Qt::transparent);
        QPainter p(&pix); drawGlyph(p,glyph,QRectF(0,0,side,side),ink); p.end();
        icon.addPixmap(pix);
    }
    return icon;
}

class PowerButton final : public QPushButton {
public:
    explicit PowerButton(QWidget *parent) : QPushButton(parent), spin(this) {
        setObjectName("power");
        setFixedSize(196,196);
        setFocusPolicy(Qt::StrongFocus);
        setAutoDefault(false);
        setCursor(Qt::PointingHandCursor);
        spin.setStartValue(0);
        spin.setEndValue(360);
        spin.setLoopCount(-1);
        spin.setDuration(1200);
        connect(&spin,&QVariantAnimation::valueChanged,this,[this] { update(); });
    }

    void showState(bool running,bool pending) {
        if (connected == running && busy == pending) return;
        connected = running;
        busy = pending;
        if (busy) spin.start(); else spin.stop();
        update();
    }

protected:
    bool hitButton(const QPoint &point) const override {
        return QLineF(QPointF(rect().center()),QPointF(point)).length() <= 84;
    }

    void paintEvent(QPaintEvent *) override {
        const bool dark = qApp->palette().color(QPalette::Window).lightness() < 128;
        const QColor accent = dark ? QColor("#c6ef66") : QColor("#507419");
        const QColor idle = dark ? QColor("#a2aaa1") : QColor("#65715f");
        const QColor face = dark ? QColor("#252b25") : QColor("#ffffff");
        const QColor border = dark ? QColor("#434c40") : QColor("#cbd2c6");
        const bool hover = isEnabled() && underMouse();
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.translate(width()/2.0,height()/2.0);
        const QRectF ring(-80,-80,160,160);
        if (hasFocus() && testAttribute(Qt::WA_KeyboardFocusChange)) {
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(accent,1.5,Qt::DashLine));
            p.drawEllipse(ring.adjusted(-8,-8,8,8));
        }
        if (isDown()) p.scale(0.97,0.97);
        if (connected || busy) {
            QColor halo = accent; halo.setAlpha(dark ? 45 : 28);
            QRadialGradient glow(QPointF(0,0),94);
            glow.setColorAt(0.82,halo);
            halo.setAlpha(0); glow.setColorAt(1.0,halo);
            p.setPen(Qt::NoPen); p.setBrush(glow); p.drawEllipse(QPointF(0,0),94,94);
        }
        p.setBrush(hover ? (dark ? face.lighter(114) : face.darker(102)) : face);
        p.setPen(QPen(connected ? accent : hover ? idle : border,3));
        p.drawEllipse(ring);
        if (busy) {
            p.setPen(QPen(accent,3,Qt::SolidLine,Qt::RoundCap));
            p.drawArc(ring, (90-spin.currentValue().toInt())*16,-100*16);
        }
        QColor symbol = connected || busy ? accent : idle;
        if (!isEnabled() && !busy) symbol.setAlpha(105);
        drawGlyph(p,Glyph::Power,QRectF(-31,-31,62,62),symbol);
    }

private:
    QVariantAnimation spin;
    bool connected = false;
    bool busy = false;
};

class SubscriptionPicker final : public QComboBox {
public:
    using QComboBox::QComboBox;
protected:
    void paintEvent(QPaintEvent *event) override {
        QComboBox::paintEvent(event);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const bool dark = qApp->palette().color(QPalette::Window).lightness() < 128;
        p.setPen(QPen(dark ? QColor("#b9c6bc") : QColor("#607166"),1.6,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
        p.translate(width()-15,height()/2.0);
        p.drawPolyline(QPolygonF{QPointF(-3,-1.5),QPointF(0,1.5),QPointF(3,-1.5)});
    }
};

class ServerRows final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem &,const QModelIndex &) const override { return {160,52}; }
    void paint(QPainter *p,const QStyleOptionViewItem &o,const QModelIndex &i) const override {
        const bool dark=qApp->palette().color(QPalette::Window).lightness()<128;
        const bool selected=o.state.testFlag(QStyle::State_Selected);
        const bool hovered=o.state.testFlag(QStyle::State_MouseOver);
        const QColor ink=dark?QColor("#edf1ed"):QColor("#202622");
        const QColor muted=dark?QColor("#a2afa6"):QColor("#65736b");
        p->save(); p->setRenderHint(QPainter::Antialiasing);
        QRectF row=o.rect.adjusted(0,1,0,-1);
        p->setPen(Qt::NoPen);
        if(selected || hovered) {
            p->setBrush(selected?(dark?QColor("#28352e"):QColor("#eaf1e8")):(dark?QColor("#262c29"):QColor("#f2f4f2")));
            p->drawRoundedRect(row,11,11);
        }
        const qreal cy=row.center().y();
        const QRectF iconBox(row.left()+13,cy-14,28,28);
        p->setBrush(dark?QColor("#38453c"):QColor("#dbe6da")); p->drawEllipse(iconBox);
        drawGlyph(*p,Glyph::Globe,iconBox.adjusted(6,6,-6,-6),muted);
        QFont font=o.font; font.setPixelSize(15); font.setWeight(QFont::DemiBold); p->setFont(font);
        QRect label=o.rect.adjusted(52,0,-44,0);
        p->setPen(ink);
        p->drawText(label,Qt::AlignVCenter|Qt::AlignLeft,QFontMetrics(font).elidedText(i.data().toString(),Qt::ElideRight,label.width()));
        if(selected) {
            const QRectF badge(row.right()-31,cy-9,18,18);
            p->setPen(Qt::NoPen); p->setBrush(dark?QColor("#bded77"):QColor("#324933")); p->drawEllipse(badge);
            drawGlyph(*p,Glyph::Check,badge.adjusted(3,3,-3,-3),dark?QColor("#1b271c"):QColor("#ffffff"));
        }
        if(o.state.testFlag(QStyle::State_HasFocus) && o.state.testFlag(QStyle::State_KeyboardFocusChange)) {
            p->setBrush(Qt::NoBrush); p->setPen(QPen(dark?QColor("#c9eb99"):QColor("#567342"),1));
            p->drawRoundedRect(row.adjusted(1,1,-1,-1),10,10);
        }
        p->restore();
    }
};
}
