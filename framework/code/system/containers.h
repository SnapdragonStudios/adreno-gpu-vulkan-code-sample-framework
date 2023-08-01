//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

struct MapBase;
struct MapElementBase;
template <class>
struct Map;

struct MapReservation {
	MapElementBase*		mSuperNode;
	long				mDirection;
};

struct MapElementBase {
	friend struct MapBase;
private:
	MapElementBase*		mSuperNode;
	MapElementBase*		mLeftSubnode;
	MapElementBase*		mRightSubnode;

	MapBase*			mOwningMap;
	long				mBalance;

	MapElementBase* First();
	MapElementBase* Last();

	void RemoveSubtree();
	void PurgeSubtree();
protected:
	MapElementBase()
	{
		mSuperNode = 0;
		mLeftSubnode = 0;
		mRightSubnode = 0;
		mOwningMap = 0;
	}

	virtual ~MapElementBase();

	MapElementBase* GetLeftSubnode() const
	{
		return mLeftSubnode;
	}

	MapElementBase* GetRightSubnode() const
	{
		return mRightSubnode;
	}

	MapBase* GetOwningMap() const
	{
		return mOwningMap;
	}

	MapElementBase* Previous() const;
	MapElementBase* Next() const;
public:
	virtual void Detach();
};

struct MapBase {
	friend struct MapElementBase;
private:
	MapElementBase*		mRootNode;

	MapElementBase*	RotateLeft(MapElementBase* node);
	MapElementBase*	RotateRight(MapElementBase* node);
	MapElementBase*	ZigZagLeft(MapElementBase* node);
	MapElementBase*	ZigZagRight(MapElementBase* node);

	void RemoveBranchNode(MapElementBase* node, MapElementBase* subnode);
protected:
	MapBase()
	{
		mRootNode = 0;
	}

	~MapBase();

	MapElementBase* operator [](long index) const;

	MapElementBase* GetRootNode() const
	{
		return mRootNode;
	}

	MapElementBase* First() const
	{
		return (mRootNode) ? mRootNode->First() : 0;
	}

	MapElementBase* Last() const
	{
		return (mRootNode) ? mRootNode->Last() : 0;
	}

	bool Member(const MapElementBase* element) const
	{
		return element->mOwningMap == this;
	}

	void SetRootNode(MapElementBase* node);

	void InsertLeftSubnode(MapElementBase* node, MapElementBase* subnode);
	void InsertRightSubnode(MapElementBase* node, MapElementBase* subnode);

	void RemoveNode(MapElementBase* node);
public:
	bool Empty() const
	{
		return !mRootNode;
	}

	long GetElementCount() const;

	void RemoveAll();
	void Purge();
};

template <class DataType>
struct MapElement : public MapElementBase {
public:
	MapElement() {}

	DataType* GetLeftSubnode() const
	{
		return static_cast<DataType*>(static_cast<MapElement<DataType>*>(MapElementBase::GetLeftSubnode()));
	}

	DataType* GetRightSubnode() const
	{
		return static_cast<DataType*>(static_cast<MapElement<DataType>*>(MapElementBase::GetRightSubnode()));
	}

	DataType* Previous() const
	{
		return static_cast<DataType*>(static_cast<MapElement<DataType>*>(MapElementBase::Previous()));
	}

	DataType* Next() const
	{
		return static_cast<DataType*>(static_cast<MapElement<DataType>*>(MapElementBase::Next()));
	}

	Map<DataType>* GetOwningMap() const
	{
		return static_cast<Map<DataType>*>(MapElementBase::GetOwningMap());
	}
};

template <class DataType>
struct Map : public MapBase {
public:
	typedef typename DataType::KeyType KeyType;

	Map() {}

	DataType* operator [](long index) const
	{
		return static_cast<DataType*>(static_cast<MapElement<DataType>*>(MapBase::operator [](index)));
	}

	DataType* First() const
	{
		return static_cast<DataType*>(static_cast<MapElement<DataType>*>(MapBase::First()));
	}

	DataType* Last() const
	{
		return static_cast<DataType*>(static_cast<MapElement<DataType>*>(MapBase::Last()));
	}

	DataType* GetRootNode() const
	{
		return static_cast<DataType*>(static_cast<MapElement<DataType>*>(MapBase::GetRootNode()));
	}

	bool Member(const MapElement<DataType>* element) const
	{
		return MapBase::Member(element);
	}

	void Remove(MapElement<DataType>* element)
	{
		RemoveNode(element);
	}

	bool Insert(DataType* element);
	void Insert(DataType* element, const MapReservation* reservation);
	bool Reserve(const KeyType& key, MapReservation* reservation);

	DataType* Find(const KeyType& key) const;
};

template <class DataType>
bool Map<DataType>::Insert(DataType* element)
{
	DataType* node = GetRootNode();
	if (node) {
		const KeyType& key = element->GetKey();
		for (;;) {
			const KeyType& nodeKey = node->GetKey();
			if (key < nodeKey) {
				DataType* subnode = node->GetLeftSubnode();
				if (!subnode) {
					InsertLeftSubnode(node, element);
					break;
				}

				node = subnode;
			}
			else if (key > nodeKey) {
				DataType* subnode = node->GetRightSubnode();
				if (!subnode) {
					InsertRightSubnode(node, element);
					break;
				}

				node = subnode;
			}
			else {
				return false;
			}
		}
	}
	else {
		SetRootNode(element);
	}

	return true;
}

template <class DataType>
void Map<DataType>::Insert(DataType* element, const MapReservation* reservation)
{
	MapElementBase* node = reservation->mSuperNode;
	if (node) {
		if (reservation->mDirection < 0) InsertLeftSubnode(node, element);
		else InsertRightSubnode(node, element);
	}
	else {
		SetRootNode(element);
	}
}

template <class DataType>
bool Map<DataType>::Reserve(const KeyType& key, MapReservation* reservation)
{
	DataType* node = GetRootNode();
	if (node) {
		for (;;) {
			const KeyType& nodeKey = node->GetKey();
			if (key < nodeKey) {
				DataType* subnode = node->GetLeftSubnode();
				if (!subnode) {
					reservation->mSuperNode = node;
					reservation->mDirection = -1;
					break;
				}

				node = subnode;
			}
			else if (key > nodeKey) {
				DataType* subnode = node->GetRightSubnode();
				if (!subnode) {
					reservation->mSuperNode = node;
					reservation->mDirection = 1;
					break;
				}

				node = subnode;
			}
			else {
				return false;
			}
		}
	}
	else {
		reservation->mSuperNode = 0;
	}

	return true;
}

template <class DataType>
DataType* Map<DataType>::Find(const KeyType& key) const
{
	DataType* node = GetRootNode();
	while (node) {
		const KeyType& nodeKey = node->GetKey();
		if (key < nodeKey) node = node->GetLeftSubnode();
		else if (key > nodeKey) node = node->GetRightSubnode();
		else break;
	}

	return (node);
}


template<typename T, unsigned int SIZE>
class PooledRing
{
public:
	PooledRing() : mCurFreeIndex(0) {}
	~PooledRing() {}
	T& GetNextFreeElement()
	{
		T& elem = mPool[mCurFreeIndex++];
		mCurFreeIndex = mCurFreeIndex < SIZE ? mCurFreeIndex : 0;
		return elem;
	}
protected:
	unsigned int	mCurFreeIndex;
	T		mPool[SIZE];
};
