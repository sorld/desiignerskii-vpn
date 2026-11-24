#pragma once

#include <QApplication>
#include <QComboBox>
#include <QFrame>
#include <QPainter>
#include <QPainterPath>
#include <QStyledItemDelegate>
#include <cmath>

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

// Render only the visible hemisphere. Cache it since traffic updates do not move the globe.
class Banner final : public QWidget {
public:
    using QWidget::QWidget;
    void setConnected(bool on) {
        if (linked == on) return;
        linked = on; artwork = {}; update();
    }
protected:
    void paintEvent(QPaintEvent *) override {
        const auto ratio = devicePixelRatioF();
        const auto pixels = (QSizeF(size()) * ratio).toSize();
        if (artwork.size() != pixels || artwork.devicePixelRatio() != ratio) {
            artwork = QPixmap(pixels); artwork.setDevicePixelRatio(ratio); artwork.fill(Qt::transparent);
            QPainter p(&artwork);
            p.setRenderHint(QPainter::Antialiasing);
            QPainterPath clip; clip.addRoundedRect(QRectF(rect()),20,20); p.setClipPath(clip);
            QLinearGradient base(0,0,width(),height());
            base.setColorAt(0,QColor("#202a27")); base.setColorAt(1,linked?QColor("#344536"):QColor("#303c37"));
            p.fillRect(rect(),base);
            if (width() >= 480) drawGlobe(p);
        }
        QPainter target(this); target.drawPixmap(0,0,artwork);
    }
private:
    void drawGlobe(QPainter &p) const {
        const double radius = qMin(height()*0.79,width()*0.205);
        const QPointF center(width()-radius*0.60,height()*0.53);
        p.save(); p.translate(center); p.rotate(-19);
        const QRectF disc(-radius,-radius,2*radius,2*radius);
        QRadialGradient glow(-radius*0.58,-radius*0.48,radius*1.85);
        glow.setColorAt(0,linked?QColor("#789260"):QColor("#586d5c"));
        glow.setColorAt(0.64,QColor("#455947")); glow.setColorAt(1,QColor("#29392e"));
        p.setBrush(glow); p.setPen(QPen(QColor(190,214,159,110),1)); p.drawEllipse(disc);
        const double tilt = 0.36;
        auto point=[=](double lat,double lon,double &depth) {
            const double x=std::cos(lat)*std::sin(lon), y=std::sin(lat), z=std::cos(lat)*std::cos(lon);
            depth=y*std::sin(tilt)+z*std::cos(tilt);
            return QPointF(radius*x,-radius*(y*std::cos(tilt)-z*std::sin(tilt)));
        };
        const double rad=std::acos(-1.0)/180.0;
        p.setBrush(Qt::NoBrush); p.setPen(QPen(QColor(192,212,171,87),0.85));
        auto line=[&](bool meridian,int constant) {
            QPainterPath path; bool visible=false;
            for(int t=meridian?-90:-180; t<=(meridian?90:180);t+=2) {
                double depth;
                QPointF v=point((meridian?t:constant)*rad,(meridian?constant:t)*rad,depth);
                if(depth>=0) { if(visible) path.lineTo(v); else path.moveTo(v); }
                visible=depth>=0;
            }
            p.drawPath(path);
        };
        for(int lon=-180;lon<180;lon+=15) line(true,lon);
        for(int lat=-75;lat<=75;lat+=15) line(false,lat);
        if(linked) {
            double z; const auto start=point(4*rad,-57*rad,z), end=point(47*rad,-5*rad,z);
            QPainterPath link(start); link.quadTo((start+end)*0.5+QPointF(-radius*0.05,-radius*0.2),end);
            p.setPen(QPen(QColor("#d5fa8a"),2.0)); p.drawPath(link);
            p.setPen(Qt::NoPen); p.setBrush(QColor(210,250,140,55)); p.drawEllipse(end,9,9);
            p.setBrush(QColor("#e0ffab")); p.drawEllipse(end,3.6,3.6); p.drawEllipse(start,2.6,2.6);
        }
        p.restore();
        // Fade the drawing into the control area on smaller windows.
        QLinearGradient shade(0,0,width(),0);
        shade.setColorAt(0,QColor("#202a27")); shade.setColorAt(0.48,QColor("#232e29"));
        shade.setColorAt(0.72,QColor(35,46,41,0)); p.fillRect(rect(),shade);
    }
    bool linked=false;
    QPixmap artwork;
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
