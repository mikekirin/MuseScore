/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore
 * Music Composition & Notation
 *
 * Copyright (C) 2024 MuseScore BVBA and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "extensionbuilder.h"

#include <QQmlEngine>

#include "global/types/number.h"
#include "global/async/async.h"

#include "../api/v1/ipluginapiv1.h"

#include "log.h"

using namespace mu::extensions;

ExtensionBuilder::ExtensionBuilder(QObject* parent)
    : QObject(parent)
{}

void ExtensionBuilder::load(const QString& uri, QObject* itemParent)
{
    Manifest m = provider()->manifest(Uri(uri.toStdString()));

    setTitle(m.title);

    //! NOTE We create extension UI using a separate engine to control what we provide,
    //! making it easier to maintain backward compatibility and stability.
    QQmlComponent component = QQmlComponent(engine()->qmlEngine(), m.qmlFilePath.toQString());
    if (!component.isReady()) {
        LOGE() << "Failed to load QML file: " << m.qmlFilePath << ", from extension: " << uri;
        LOGE() << component.errorString();
        return;
    }

    QObject* obj = component.createWithInitialProperties({ { "parent", QVariant::fromValue(itemParent) } });

    m_contentItem = qobject_cast<QQuickItem*>(obj);
    if (!m_contentItem) {
        LOGE() << "Component not QuickItem, file: " << m.qmlFilePath << ", from extension: " << uri;
    }

    if (m_contentItem) {
        if (mu::is_zero(m_contentItem->implicitHeight())) {
            m_contentItem->setImplicitHeight(m_contentItem->height());
            if (mu::is_zero(m_contentItem->implicitHeight())) {
                m_contentItem->setImplicitHeight(480);
            }
        }

        if (mu::is_zero(m_contentItem->implicitWidth())) {
            m_contentItem->setImplicitWidth(m_contentItem->width());
            if (mu::is_zero(m_contentItem->implicitWidth())) {
                m_contentItem->setImplicitWidth(600);
            }
        }
    }

    emit contentItemChanged();

    //! NOTE For version 1 plugins we need to call run
    if (m.apiversion == 1) {
        async::Async::call(this, [this, m]() {
            apiv1::IPluginApiV1* plugin = dynamic_cast<apiv1::IPluginApiV1*>(m_contentItem);
            if (!plugin) {
                LOGE() << "Qml Object not MuseScore plugin: " << m.qmlFilePath
                       << ", from extension: " << m.uri.toString();
                return;
            }

            plugin->runPlugin();
        });
    }
}

QQuickItem* ExtensionBuilder::contentItem() const
{
    return m_contentItem;
}

void ExtensionBuilder::setTitle(QString title)
{
    if (m_title == title) {
        return;
    }
    m_title = title;
    emit titleChanged();
}

QString ExtensionBuilder::title() const
{
    return m_title;
}
