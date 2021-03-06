//  Natron
//
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * Created by Alexandre GAUTHIER-FOICHAT on 6/1/2012.
 * contact: immarespond at gmail dot com
 *
 */

#ifndef TOOLBUTTON_H
#define TOOLBUTTON_H

// from <https://docs.python.org/3/c-api/intro.html#include-files>:
// "Since Python may define some pre-processor definitions which affect the standard headers on some systems, you must include Python.h before any standard headers are included."
#include <Python.h>

#include "Global/Macros.h"
CLANG_DIAG_OFF(deprecated)
CLANG_DIAG_OFF(uninitialized)
#include <QtCore/QObject>
#include <QIcon>
CLANG_DIAG_ON(deprecated)
CLANG_DIAG_ON(uninitialized)
#if !defined(Q_MOC_RUN) && !defined(SBK_RUN)
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#endif
class PluginGroupNode;
class GuiAppInstance;

class QMenu;
class QAction;

struct ToolButtonPrivate;
class ToolButton
    : public QObject
{
    Q_OBJECT

public:


    ToolButton( GuiAppInstance* app,
                const boost::shared_ptr<PluginGroupNode>& pluginToolButton,
                const QString & pluginID,
                int major,
                int minor,
                const QString & label,
                QIcon icon = QIcon() );

    virtual ~ToolButton();

    const QString & getID() const;
    
    int getPluginMajor() const;
    
    int getPluginMinor() const;
    
    const QString & getLabel() const;
    const QIcon & getIcon() const;

    bool hasChildren() const;

    QMenu* getMenu() const;

    void setMenu(QMenu* menu );

    void tryAddChild(ToolButton* child);

    const std::vector<ToolButton*> & getChildren() const;
    QAction* getAction() const;

    void setAction(QAction* action);

    boost::shared_ptr<PluginGroupNode> getPluginToolButton() const;

public Q_SLOTS:

    void onTriggered();

private:

    boost::scoped_ptr<ToolButtonPrivate> _imp;
};

#endif // TOOLBUTTON_H
