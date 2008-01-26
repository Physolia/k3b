/*
 *
 * Copyright (C) 2008 Sebastian Trueg <trueg@k3b.org>
 *
 * This file is part of the K3b project.
 * Copyright (C) 1998-2008 Sebastian Trueg <trueg@k3b.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * See the file "COPYING" for the exact licensing terms.
 */

#ifndef _K3B_META_ITEM_MODEL_H_
#define _K3B_META_ITEM_MODEL_H_

#include <QAbstractItemModel>

#include <KUrl>

class KIcon;

// TODO: * implement the mimestuff
//       * Have a K3bMetaItemView which allows to set delegates for submodel header painting
//       * implement something like modelHeaderData() to get data for the root elements

/**
 * Meta item model which combines multiple submodels into
 * one big model.
 *
 * Usage is very simple: just call addSubModel for each 
 * model that should be added to the meta model.
 */
class K3bMetaItemModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    K3bMetaItemModel( QObject* parent = 0 );
    ~K3bMetaItemModel();

    QAbstractItemModel* subModelForIndex( const QModelIndex& index ) const;

    /**
     * Map index to an index used in the submodel. The returned index
     * should be used carefully.
     */
    QModelIndex mapToSubModel( const QModelIndex& index ) const;
    QModelIndex mapFromSubModel( const QModelIndex& index ) const;

    /**
     * Always returns 1 as K3bMetaItemModel does not support multiple columns yet
     */
    virtual int columnCount( const QModelIndex& parent = QModelIndex() ) const;

    QVariant data( const QModelIndex& index, int role = Qt::DisplayRole ) const;
    QModelIndex index( int row, int column, const QModelIndex& parent = QModelIndex() ) const;
    QModelIndex parent( const QModelIndex& index ) const;
    int rowCount( const QModelIndex& parent = QModelIndex() ) const;
    Qt::ItemFlags flags( const QModelIndex& index ) const;
    bool hasChildren( const QModelIndex& parent = QModelIndex() ) const;
    bool canFetchMore( const QModelIndex& parent ) const;
    void fetchMore( const QModelIndex& parent );
    bool setData( const QModelIndex& index, const QVariant& value, int role = Qt::EditRole );
    bool dropMimeData( const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent );

    /**
     * Can handle lists of indexes from a single submodel. Mixing indexes
     * from different submodels is not supported yet and results in the method
     * returning 0.
     */
    virtual QMimeData* mimeData( const QModelIndexList& indexes ) const;

    /**
     * The default implementation just returns the list of all drop actions
     * supported by any of the submodels.
     */
    virtual Qt::DropActions supportedDropActions() const;

public Q_SLOTS:
    /**
     * K3bPlacesModel takes over ownership of model.
     * FIXME: name and icon are weird parameters here
     *
     * \param model The submodel to be added.
     * \param flat If flat is set true the root items of the submodel will
     * be merged into the root item list of this model. Otherwise the submodel
     * will be added under a new root item.
     */
    void addSubModel( const QString& name, const KIcon& icon, QAbstractItemModel* model, bool flat = false );

    /**
     * FIXME: better use an id or something?
     */
    void removeSubModel( QAbstractItemModel* model );

private Q_SLOTS:
    void slotRowsAboutToBeInserted( const QModelIndex&, int, int );
    void slotRowsInserted( const QModelIndex&, int, int );
    void slotRowsAboutToBeRemoved( const QModelIndex&, int, int );
    void slotRowsRemoved( const QModelIndex&, int, int );
    void slotDataChanged( const QModelIndex&, const QModelIndex& );
    void slotReset();

private:
    class Private;
    Private* const d;
};

#endif
