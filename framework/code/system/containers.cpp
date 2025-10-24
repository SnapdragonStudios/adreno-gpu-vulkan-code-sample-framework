//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#include "containers.h"

inline long Abs(long x)
{
	long a = x >> 31;
	return (x ^ a) - a;
}

inline long MinZero(long x)
{
	return x & (x >> 31);
}

inline long MaxZero(long x)
{
	return x & ~(x >> 31);
}

MapElementBase::~MapElementBase()
{
	if (mOwningMap) mOwningMap->RemoveNode(this);
}

void MapElementBase::Detach()
{
	if (mOwningMap) mOwningMap->RemoveNode(this);
}

MapElementBase* MapElementBase::First()
{
	MapElementBase* element = this;
	for (;;) {
		MapElementBase* left = element->mLeftSubnode;
		if (!left) break;
		element = left;
	}

	return element;
}

MapElementBase* MapElementBase::Last()
{
	MapElementBase* element = this;
	for (;;) {
		MapElementBase* right = element->mRightSubnode;
		if (!right) break;
		element = right;
	}

	return element;
}

MapElementBase* MapElementBase::Previous() const
{
	if (mLeftSubnode) return mLeftSubnode->Last();

	const MapElementBase* element = this;
	for (;;) {
		MapElementBase* super = element->mSuperNode;
		if (!super) break;

		if (super->mRightSubnode == element) return super;
		element = super;
	}

	return 0;
}

MapElementBase* MapElementBase::Next() const
{
	if (mRightSubnode) return mRightSubnode->First();

	const MapElementBase* element = this;
	for (;;) {
		MapElementBase* super = element->mSuperNode;
		if (!super) break;

		if (super->mLeftSubnode == element) return super;
		element = super;
	}

	return 0;
}

void MapElementBase::RemoveSubtree()
{
	if (mLeftSubnode) mLeftSubnode->RemoveSubtree();
	if (mRightSubnode) mRightSubnode->RemoveSubtree();

	mSuperNode = 0;
	mLeftSubnode = 0;
	mRightSubnode = 0;
	mOwningMap = 0;
}

void MapElementBase::PurgeSubtree()
{
	if (mLeftSubnode) mLeftSubnode->PurgeSubtree();
	if (mRightSubnode) mRightSubnode->PurgeSubtree();

	mOwningMap = 0;
	delete this;
}

MapBase::~MapBase()
{
	Purge();
}

MapElementBase* MapBase::operator [](long index) const
{
	int i = 0;
	MapElementBase* element = First();
	while (element) {
		if (i == index) return element;
		element = element->Next();
		i++;
	}

	return 0;
}

long MapBase::GetElementCount() const
{
	int count = 0;
	const MapElementBase* element = First();
	while (element) {
		count++;
		element = element->Next();
	}

	return long(count);
}

MapElementBase* MapBase::RotateLeft(MapElementBase* node)
{
	MapElementBase* right = node->mRightSubnode;

	if (node != mRootNode) {
		MapElementBase* super = node->mSuperNode;
		if (super->mLeftSubnode == node) super->mLeftSubnode = right;
		else super->mRightSubnode = right;
		right->mSuperNode = super;
	}
	else {
		mRootNode = right;
		right->mSuperNode = 0;
	}

	MapElementBase* subnode = right->mLeftSubnode;
	if (subnode) subnode->mSuperNode = node;
	node->mRightSubnode = subnode;

	right->mLeftSubnode = node;
	node->mSuperNode = right;
	node->mBalance = -(--right->mBalance);

	return right;
}

MapElementBase* MapBase::RotateRight(MapElementBase* node)
{
	MapElementBase* left = node->mLeftSubnode;

	if (node != mRootNode) {
		MapElementBase* super = node->mSuperNode;
		if (super->mLeftSubnode == node) super->mLeftSubnode = left;
		else super->mRightSubnode = left;
		left->mSuperNode = super;
	}
	else {
		mRootNode = left;
		left->mSuperNode = 0;
	}

	MapElementBase* subnode = left->mRightSubnode;
	if (subnode) subnode->mSuperNode = node;
	node->mLeftSubnode = subnode;

	left->mRightSubnode = node;
	node->mSuperNode = left;
	node->mBalance = -(++left->mBalance);

	return left;
}

MapElementBase* MapBase::ZigZagLeft(MapElementBase* node)
{
	MapElementBase* right = node->mRightSubnode;
	MapElementBase* top = right->mLeftSubnode;

	if (node != mRootNode) {
		MapElementBase* super = node->mSuperNode;
		if (super->mLeftSubnode == node) super->mLeftSubnode = top;
		else super->mRightSubnode = top;
		top->mSuperNode = super;
	}
	else {
		mRootNode = top;
		top->mSuperNode = 0;
	}

	MapElementBase* subLeft = top->mLeftSubnode;
	if (subLeft) subLeft->mSuperNode = node;
	node->mRightSubnode = subLeft;

	MapElementBase* subRight = top->mRightSubnode;
	if (subRight) subRight->mSuperNode = right;
	right->mLeftSubnode = subRight;

	top->mLeftSubnode = node;
	top->mRightSubnode = right;
	node->mSuperNode = top;
	right->mSuperNode = top;

	long b = top->mBalance;
	node->mBalance = -MaxZero(b);
	right->mBalance = -MinZero(b);
	top->mBalance = 0;

	return top;
}

MapElementBase* MapBase::ZigZagRight(MapElementBase* node)
{
	MapElementBase* left = node->mLeftSubnode;
	MapElementBase* top = left->mRightSubnode;

	if (node != mRootNode) {
		MapElementBase* super = node->mSuperNode;
		if (super->mLeftSubnode == node) super->mLeftSubnode = top;
		else super->mRightSubnode = top;
		top->mSuperNode = super;
	}
	else {
		mRootNode = top;
		top->mSuperNode = 0;
	}

	MapElementBase* subLeft = top->mLeftSubnode;
	if (subLeft) subLeft->mSuperNode = left;
	left->mRightSubnode = subLeft;

	MapElementBase* subRight = top->mRightSubnode;
	if (subRight) subRight->mSuperNode = node;
	node->mLeftSubnode = subRight;

	top->mLeftSubnode = left;
	top->mRightSubnode = node;
	node->mSuperNode = top;
	left->mSuperNode = top;

	long b = top->mBalance;
	node->mBalance = -MinZero(b);
	left->mBalance = -MaxZero(b);
	top->mBalance = 0;

	return top;
}

void MapBase::SetRootNode(MapElementBase* node)
{
	MapBase* map = node->mOwningMap;
	if (map) map->RemoveNode(node);

	node->mOwningMap = this;
	node->mBalance = 0;

	mRootNode = node;
}

void MapBase::InsertLeftSubnode(MapElementBase* node, MapElementBase* subnode)
{
	MapBase* map = subnode->mOwningMap;
	if (map) map->RemoveNode(subnode);

	node->mLeftSubnode = subnode;
	subnode->mSuperNode = node;
	subnode->mOwningMap = this;
	subnode->mBalance = 0;

	long b = node->mBalance - 1;
	node->mBalance = b;
	if (b != 0) {
		long dir1 = -1;
		for (;;) {
			long	dir2;

			MapElementBase* super = node->mSuperNode;
			if (!super) break;

			b = super->mBalance;
			if (super->mLeftSubnode == node) {
				super->mBalance = --b;
				dir2 = -1;
			}
			else {
				super->mBalance = ++b;
				dir2 = 1;
			}

			if (b == 0) break;

			if (Abs(b) == 2) {
				if (dir2 == -1) {
					if (dir1 == -1) RotateRight(super);
					else ZigZagRight(super);
				}
				else {
					if (dir1 == 1) RotateLeft(super);
					else ZigZagLeft(super);
				}

				break;
			}

			dir1 = dir2;
			node = super;
		}
	}
}

void MapBase::InsertRightSubnode(MapElementBase* node, MapElementBase* subnode)
{
	MapBase* map = subnode->mOwningMap;
	if (map) map->RemoveNode(subnode);

	node->mRightSubnode = subnode;
	subnode->mSuperNode = node;
	subnode->mOwningMap = this;
	subnode->mBalance = 0;

	long b = node->mBalance + 1;
	node->mBalance = b;
	if (b != 0) {
		long dir1 = 1;
		for (;;) {
			long	dir2;

			MapElementBase* super = node->mSuperNode;
			if (!super) break;

			b = super->mBalance;
			if (super->mLeftSubnode == node) {
				super->mBalance = --b;
				dir2 = -1;
			}
			else {
				super->mBalance = ++b;
				dir2 = 1;
			}

			if (b == 0) break;

			if (Abs(b) == 2) {
				if (dir2 == -1) {
					if (dir1 == -1) RotateRight(super);
					else ZigZagRight(super);
				}
				else {
					if (dir1 == 1) RotateLeft(super);
					else ZigZagLeft(super);
				}

				break;
			}

			dir1 = dir2;
			node = super;
		}
	}
}

void MapBase::RemoveBranchNode(MapElementBase* node, MapElementBase* subnode)
{
	MapElementBase* super = node->mSuperNode;
	if (subnode) subnode->mSuperNode = super;

	if (super) {
		long	db;

		if (super->mLeftSubnode == node) {
			super->mLeftSubnode = subnode;
			db = 1;
		}
		else {
			super->mRightSubnode = subnode;
			db = -1;
		}

		for (;;) {
			long b = (super->mBalance += db);
			if (Abs(b) == 1) break;

			node = super;
			super = super->mSuperNode;

			if (b != 0) {
				if (b > 0) {
					long rb = node->mRightSubnode->mBalance;
					if (rb >= 0) {
						node = RotateLeft(node);
						if (rb == 0) break;
					}
					else {
						node = ZigZagLeft(node);
					}
				}
				else {
					long lb = node->mLeftSubnode->mBalance;
					if (lb <= 0) {
						node = RotateRight(node);
						if (lb == 0) break;
					}
					else {
						node = ZigZagRight(node);
					}
				}
			}

			if (!super) break;
			db = (super->mLeftSubnode == node) ? 1 : -1;
		}
	}
	else {
		mRootNode = subnode;
	}
}

void MapBase::RemoveNode(MapElementBase* node)
{
	MapElementBase* left = node->mLeftSubnode;
	MapElementBase* right = node->mRightSubnode;

	if ((left) && (right)) {
		MapElementBase* top = right->First();
		RemoveBranchNode(top, top->mRightSubnode);

		MapElementBase* super = node->mSuperNode;
		top->mSuperNode = super;
		if (super) {
			if (super->mLeftSubnode == node) super->mLeftSubnode = top;
			else super->mRightSubnode = top;
		}
		else {
			mRootNode = top;
		}

		left = node->mLeftSubnode;
		if (left) left->mSuperNode = top;
		top->mLeftSubnode = left;

		right = node->mRightSubnode;
		if (right) right->mSuperNode = top;
		top->mRightSubnode = right;

		top->mBalance = node->mBalance;
	}
	else {
		RemoveBranchNode(node, (left) ? left : right);
	}

	node->mSuperNode = 0;
	node->mLeftSubnode = 0;
	node->mRightSubnode = 0;
	node->mOwningMap = 0;
}

void MapBase::RemoveAll()
{
	if (mRootNode) {
		mRootNode->RemoveSubtree();
		mRootNode = 0;
	}
}

void MapBase::Purge()
{
	if (mRootNode) {
		mRootNode->PurgeSubtree();
		mRootNode = 0;
	}
}