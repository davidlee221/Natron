//  Natron
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * Created by Alexandre GAUTHIER-FOICHAT on 6/1/2012.
 * contact: immarespond at gmail dot com
 *
 */


#include "DefaultOverlays.h"
#include <list>
#include <cmath>

#include "Gui/NodeGui.h"
#include "Gui/TextRenderer.h"
#include "Gui/GuiApplicationManager.h"

#include "Engine/KnobTypes.h"
#include "Engine/Settings.h"

#include "Global/KeySymbols.h"

#include "Global/Macros.h"
#include "Global/GLIncludes.h" //!<must be included before QGlWidget because of gl.h and glew.h
CLANG_DIAG_OFF(deprecated)
#include <QtOpenGL/QGLWidget>
CLANG_DIAG_ON(deprecated)
#include <QPointF>
#include <QThread>
#include <QFont>
#include <QColor>
#include <QApplication>

namespace {
enum PositionInteractState
{
    ePositionInteractStateInactive,
    ePositionInteractStatePoised,
    ePositionInteractStatePicked
};

struct PositionInteract
{
    boost::shared_ptr<Double_Knob> param;
    QPointF dragPos;
    PositionInteractState state;
    
    
    double pointSize() const
    {
        return 5;
    }
    
    double pointTolerance() const
    {
        return 6.;
    }
    
    
};
   
// round to the closest int, 1/10 int, etc
// this make parameter editing easier
// pscale is args.pixelScale.x / args.renderScale.x;
// pscale10 is the power of 10 below pscale
inline double fround(double val,
                         double pscale)
{
    double pscale10 = std::pow( 10.,std::floor( std::log10(pscale) ) );
    
    return pscale10 * std::floor(val / pscale10 + 0.5);
}
    
typedef std::list<PositionInteract> PositionInteracts;
    
}

struct DefaultOverlayPrivate
{
    PositionInteracts positions;
    boost::weak_ptr<NodeGui> node;
    
    QPointF lastPenPos;
    
    Natron::TextRenderer textRenderer;
    
    bool interactiveDrag;
    
    DefaultOverlayPrivate(const boost::shared_ptr<NodeGui>& node)
    : positions()
    , node(node)
    , lastPenPos()
    , textRenderer()
    , interactiveDrag(false)
    {
        
    }
    
};

DefaultOverlay::DefaultOverlay(const boost::shared_ptr<NodeGui>& node)
: _imp(new DefaultOverlayPrivate(node))
{
}

DefaultOverlay::~DefaultOverlay()
{
    
}

boost::shared_ptr<NodeGui>
DefaultOverlay::getNode() const
{
    return _imp->node.lock();
}

bool
DefaultOverlay::addPositionParam(const boost::shared_ptr<Double_Knob>& position)
{
    assert(QThread::currentThread() == qApp->thread());
    
    for (PositionInteracts::iterator it = _imp->positions.begin(); it != _imp->positions.end(); ++it) {
        if (it->param == position) {
            return false;
        }
    }
    
    PositionInteract p;
    p.param = position;
    p.state = ePositionInteractStateInactive;
    _imp->positions.push_back(p);
    return true;
}


void
DefaultOverlay::draw(double time,const RenderScale& /*renderScale*/)
{
    double r,g,b;
    if (!getNode()->getOverlayColor(&r, &g, &b)) {
        r = g = b = 0.8;
    }
    OfxPointD pscale;
    n_getPixelScale(pscale.x, pscale.y);
    double w,h;
    n_getViewportSize(w, h);
    
    
    GLdouble projection[16];
    glGetDoublev( GL_PROJECTION_MATRIX, projection);
    OfxPointD shadow; // how much to translate GL_PROJECTION to get exactly one pixel on screen
    shadow.x = 2./(projection[0] * w);
    shadow.y = 2./(projection[5] * h);
    
    QFont font(appFont,appFontSize);
    QFontMetrics fm(font);
    for (PositionInteracts::iterator it = _imp->positions.begin(); it != _imp->positions.end(); ++it) {
        
        double pR,pG,pB;
        switch (it->state) {
        case ePositionInteractStateInactive:
            pR = (float)r; pG = (float)g; pB = (float)b; break;
        case ePositionInteractStatePoised:
            pR = 0.f; pG = 1.0f; pB = 0.0f; break;
        case ePositionInteractStatePicked:
            pR = 0.f; pG = 1.0f; pB = 0.0f; break;
        }
        
        QPointF pos;
        if (it->state == ePositionInteractStatePicked) {
            pos = _imp->lastPenPos;
        } else {
            pos.rx() = it->param->getValueAtTime(time, 0);
            pos.ry() = it->param->getValueAtTime(time, 1);
        }
        //glPushAttrib(GL_ALL_ATTRIB_BITS); // caller is responsible for protecting attribs
        glPointSize( (GLfloat)it->pointSize() );
        // Draw everything twice
        // l = 0: shadow
        // l = 1: drawing
        for (int l = 0; l < 2; ++l) {
            // shadow (uses GL_PROJECTION)
            glMatrixMode(GL_PROJECTION);
            int direction = (l == 0) ? 1 : -1;
            // translate (1,-1) pixels
            glTranslated(direction * shadow.x, -direction * shadow.y, 0);
            glMatrixMode(GL_MODELVIEW); // Modelview should be used on Nuke
            
            glColor3f(pR * l, pG * l, pB * l);
            glBegin(GL_POINTS);
            glVertex2d(pos.x(), pos.y());
            glEnd();
            QColor c;
            c.setRgbF(pR * l, pG * l, pB * l);
            _imp->textRenderer.renderText(pos.x(), pos.y() - (fm.height() + it->pointSize()) * pscale.y,
                                          pscale.x, pscale.y, QString(it->param->getDescription().c_str()), c, font);
        }
    }

}

bool
DefaultOverlay::penMotion(double time,
               const RenderScale &/*renderScale*/,
               const QPointF &penPos,
               const QPoint &/*penPosViewport*/,
               double /*pressure*/)
{
    OfxPointD pscale;
    n_getPixelScale(pscale.x, pscale.y);
    
    for (PositionInteracts::iterator it = _imp->positions.begin(); it!= _imp->positions.end(); ++it) {
        QPointF pos;
        if (it->state == ePositionInteractStatePicked) {
            pos = _imp->lastPenPos;
        } else {
            pos.rx() = it->param->getValueAtTime(time, 0);
            pos.ry() = it->param->getValueAtTime(time, 1);
        }
        
        bool didSomething = false;
        bool valuesChanged = false;
        
        switch (it->state) {
            case ePositionInteractStateInactive:
            case ePositionInteractStatePoised: {
                // are we in the box, become 'poised'
                PositionInteractState newState;
                if ( ( std::fabs(penPos.x() - pos.x()) <= it->pointTolerance() * pscale.x) &&
                    ( std::fabs(penPos.y() - pos.y()) <= it->pointTolerance() * pscale.y) ) {
                    newState = ePositionInteractStatePoised;
                } else   {
                    newState = ePositionInteractStateInactive;
                }
                
                //if (it->state != newState) { //< causes the viewer not to redraw if pressed twice while poised
                    it->state = newState;
                    didSomething = true;
                //}
            }
                break;
                
            case ePositionInteractStatePicked: {
                _imp->lastPenPos = penPos;
                valuesChanged = true;
            }
                break;
        }
        
        if (it->state != ePositionInteractStateInactive && _imp->interactiveDrag && valuesChanged) {
            double x = fround(_imp->lastPenPos.x(), pscale.x);
            double y = fround(_imp->lastPenPos.y(), pscale.y);
            it->param->setValue(x , 0);
            it->param->setValue(y , 1);
        }
        if (didSomething || valuesChanged) {
            return true;
        }
    }
 
    return false;
}


bool
DefaultOverlay::penUp(double time,
           const RenderScale &renderScale,
           const QPointF &penPos,
           const QPoint &penPosViewport,
           double  pressure)
{
    OfxPointD pscale;
    n_getPixelScale(pscale.x, pscale.y);
    
    for (PositionInteracts::iterator it = _imp->positions.begin(); it != _imp->positions.end(); ++it) {

        double x = fround(_imp->lastPenPos.x(), pscale.x);
        double y = fround(_imp->lastPenPos.y(), pscale.y);
        it->param->setValue(x , 0);
        it->param->setValue(y , 1);
        
        it->state = ePositionInteractStatePoised;
        penMotion(time,renderScale,penPos,penPosViewport,pressure);
        
        return true;
        
    }
    return false;

}


bool
DefaultOverlay::penDown(double time,
             const RenderScale &renderScale,
             const QPointF &penPos,
             const QPoint &penPosViewport,
             double  pressure)
{
  
    bool didSomething = false;
    
    for (PositionInteracts::iterator it = _imp->positions.begin(); it != _imp->positions.end(); ++it) {
        penMotion(time,renderScale,penPos,penPosViewport,pressure);
        if (it->state == ePositionInteractStatePoised) {
            it->state = ePositionInteractStatePicked;
            _imp->lastPenPos = penPos;
            if (_imp->interactiveDrag) {
                _imp->interactiveDrag = appPTR->getCurrentSettings()->getRenderOnEditingFinishedOnly();
            }
            didSomething = true;
        }
        
        if (didSomething) {
            return true;
        }

    }
    return false;
}


bool
DefaultOverlay::keyDown(double /*time*/,
             const RenderScale &/*renderScale*/,
             int     /*key*/,
             char*   /*keyString*/)
{
    return false;
}


bool
DefaultOverlay::keyUp(double /*time*/,
           const RenderScale &/*renderScale*/,
           int     /*key*/,
           char*   /*keyString*/)
{
    return false;
}


bool
DefaultOverlay::keyRepeat(double /*time*/,
               const RenderScale &/*renderScale*/,
               int     /*key*/,
               char*   /*keyString*/)
{
    return false;
}


bool
DefaultOverlay::gainFocus(double /*time*/,
               const RenderScale &/*renderScale*/)
{
    return false;
}


bool
DefaultOverlay::loseFocus(double  /*time*/,
               const RenderScale &/*renderScale*/)
{
    return false;
}

bool
DefaultOverlay::hasDefaultOverlayForParam(const KnobI* param)
{
    assert(QThread::currentThread() == qApp->thread());
    for (PositionInteracts::iterator it = _imp->positions.begin(); it != _imp->positions.end(); ++it) {
        if (it->param.get() == param) {
            return true;
        }
    }
    return false;
}

void
DefaultOverlay::removeDefaultOverlay(KnobI* knob)
{
    for (PositionInteracts::iterator it = _imp->positions.begin(); it != _imp->positions.end(); ++it) {
        if (it->param.get() == knob) {
            _imp->positions.erase(it);
            return;
        }
    }
}

bool
DefaultOverlay::isEmpty() const
{
    if (_imp->positions.empty()) {
        return true;
    }
    return false;
}
