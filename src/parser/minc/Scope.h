//
//  Scope.h
//  RTcmix
//
//  Created by Douglas Scott on 12/30/19.
//

#ifndef Scope_h
#define Scope_h

#include "rt_types.h"
#include "minc_internal.h"

class Scope;
typedef std::vector<Scope *>ScopeStack;
typedef std::vector<ScopeStack *>CallStack;


class ScopeManager
{
    static ScopeStack *sScopeStack;
public:
    static ScopeStack *stack();
    static void setStack(ScopeStack *stack);
    static Scope *globalScope();
    static Scope *currentScope();
    static void dump();
    static void destroy();
};

// MemberInfo describes a member in a MinC-declared struct

struct MemberInfo {
    MemberInfo(const char *inName, MincDataType inType) : name(inName), type(inType) {}
    const char *    name;
    MincDataType    type;
};

// A StructType describes a MinC-declared struct

class StructType {
public:
    StructType(const char *inName) : _name(inName) {}
    ~StructType() {}
    void addElement(const char *name, MincDataType type) {
        // TODO: Dont allow duplicate element names
        _members.push_back(MemberInfo(name, type));
    }
    template <typename FuncType>
    void forEachElement(FuncType &function) const {
        for (std::vector<MemberInfo>::const_iterator i = _members.begin(); i != _members.end(); ++i) {
            function(i->name, i->type);
        }
    }
    const char *name() const { return _name; }
protected:
    const char *                _name;
    std::vector<MemberInfo>     _members;
};

StructType *installType(const char *typeName, Bool isGlobal);
const StructType *lookupType(const char *typeName, LookupType lookupType);

void push_function_stack();
void pop_function_stack();
void push_scope();
void pop_scope();
int current_scope();
void restore_scope(int scope);

class Symbol;
Symbol *installSymbol(const char *name, Bool isGlobal);
Symbol *lookupSymbol(const char *name, LookupType lookupType);
Symbol * lookupOrAutodeclare(const char *name, Bool inFunctionCall);

void dump_symbols();

void free_scopes();

#endif /* Scope_h */
