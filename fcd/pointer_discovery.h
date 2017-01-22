//
// pass_pointerdiscovery.h
// Copyright (C) 2015 Félix Cloutier.
// All Rights Reserved.
//
// This file is part of fcd.
// 
// fcd is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// fcd is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with fcd.  If not, see <http://www.gnu.org/licenses/>.
//

#ifndef pass_pointerdiscovery_h
#define pass_pointerdiscovery_h

#include "dumb_allocator.h"
#include "executable.h"
#include "not_null.h"

#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/Support/raw_ostream.h>

#include <deque>
#include <unordered_map>
#include <unordered_set>

struct ObjectAddress;
struct RootObjectAddress;
typedef NOT_NULL(std::unordered_set<ObjectAddress*>) UnificationSet;

namespace
{
	class ConstraintContext;
}

typedef std::pair<int64_t, unsigned> ObjectAddressOrderingKey;

struct ObjectAddress
{
	enum Type
	{
		Root,
		ConstantOffset,
		VariableOffset,
	};
	
	NOT_NULL(llvm::Value) value;
	UnificationSet unification;
	Type type;
	
	ObjectAddress(Type type, NOT_NULL(llvm::Value) value, UnificationSet unification)
	: type(type), value(value), unification(unification)
	{
	}
	
	virtual RootObjectAddress& getRoot() = 0;
	virtual ObjectAddressOrderingKey getOrderingKey() const = 0;
	virtual void print(llvm::raw_ostream& os) const = 0;
	void dump() const;
};

struct RootObjectAddress : public ObjectAddress
{
	static bool classof(const ObjectAddress* address)
	{
		return address->type == Root;
	}
	
	RootObjectAddress(NOT_NULL(llvm::Value) value, UnificationSet unification)
	: ObjectAddress(Root, value, unification)
	{
	}
	
	virtual RootObjectAddress& getRoot() override;
	virtual ObjectAddressOrderingKey getOrderingKey() const override;
	virtual void print(llvm::raw_ostream& os) const override;
};

struct RelativeObjectAddress : public ObjectAddress
{
	NOT_NULL(ObjectAddress) parent;
	
	RelativeObjectAddress(Type type, NOT_NULL(llvm::Value) value, UnificationSet unification, NOT_NULL(ObjectAddress) parent);
	
	virtual RootObjectAddress& getRoot() override final;
};

struct ConstantOffsetObjectAddress : public RelativeObjectAddress
{
	static bool classof(const ObjectAddress* address)
	{
		return address->type == ConstantOffset;
	}
	
	int64_t offset;
	
	ConstantOffsetObjectAddress(NOT_NULL(llvm::Value) value, UnificationSet unification, NOT_NULL(ObjectAddress) parent, int64_t offset)
	: RelativeObjectAddress(ConstantOffset, value, unification, parent), offset(offset)
	{
	}
	
	virtual ObjectAddressOrderingKey getOrderingKey() const override;
	virtual void print(llvm::raw_ostream& os) const override;
};

struct VariableOffsetObjectAddress : public RelativeObjectAddress
{
	static bool classof(const ObjectAddress* address)
	{
		return address->type == VariableOffset;
	}
	
	NOT_NULL(llvm::Value) index;
	uint64_t stride;
	
	VariableOffsetObjectAddress(NOT_NULL(llvm::Value) value, UnificationSet unification, NOT_NULL(ObjectAddress) parent, NOT_NULL(llvm::Value) index, uint64_t stride)
	: RelativeObjectAddress(VariableOffset, value, unification, parent), index(index), stride(stride)
	{
	}
	
	virtual ObjectAddressOrderingKey getOrderingKey() const override;
	virtual void print(llvm::raw_ostream& os) const override;
};

// Find all the pointers in a module, identify which pointers should/may point to the same type of memory.
class PointerDiscovery
{
	DumbAllocator pool;
	
	std::unique_ptr<ConstraintContext> context;
	const std::unordered_set<llvm::Value*>* pointerValues;
	llvm::Function* currentFunction;
	
	std::unordered_map<const void*, std::unordered_set<ObjectAddress*>> sameTypeSets;
	std::unordered_map<llvm::Value*, ObjectAddress*> objectAddresses;
	std::unordered_map<llvm::Function*, std::deque<ObjectAddress*>> addressesByFunction;
	
	template<typename AddressType, typename... Arguments>
	AddressType& createAddress(llvm::Value& value, Arguments&&... args);
	
	ObjectAddress& handleAddition(ObjectAddress& base, llvm::BinaryOperator& totalValue, llvm::Value& added, bool positive);
	ObjectAddress& createAddressHierarchy(llvm::Value& value);
	
public:
	PointerDiscovery();
	~PointerDiscovery();
	
	void analyzeModule(Executable& executable, llvm::Module& module);
	
	ObjectAddress* getAddressOfArgument(llvm::Argument& arg)
	{
		auto iter = objectAddresses.find(&arg);
		return iter == objectAddresses.end() ? nullptr : iter->second;
	}
	
	const std::deque<ObjectAddress*>* getAddressesInFunction(llvm::Function& fn) const
	{
		auto iter = addressesByFunction.find(&fn);
		if (iter != addressesByFunction.end())
		{
			return &iter->second;
		}
		return nullptr;
	}
};

#endif /* pass_pointerdiscovery_hpp */
