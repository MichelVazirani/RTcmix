//
//  Symbol.h
//  RTcmix
//
//  Created by Douglas Scott on 12/30/19.
//

#ifndef Symbol_h
#define Symbol_h

#include "MincValue.h"
#include "minc_internal.h"

class Node;
class StructType;

class Symbol {               /* symbol table entries */
public:
    static Symbol *    create(const char *name);
    ~Symbol();
    void                init(const StructType *);
    MincDataType        dataType() const { return v.dataType(); }
    const MincValue&    value() const { return v; }
    MincValue&            value() { return v; }
    const char *        name() { return _name; }
    Node *              node() { return _node; }
    void                setNode(Node *inNode) { _node = inNode; }
    
    Symbol *            copyValue(Node *);
    
    Symbol *            getStructMember(const char *memberName);
    
    void                dump();
    
    Symbol *next;                 /* next entry on hash chain */
    int scope;
protected:
    Symbol(const char *name);
    const char *_name;          /* symbol name */
    Node *      _node;              /* for symbols that are functions, function def */
    MincValue   v;
#ifdef NOTYET
    short defined;             /* set when function defined */
    short offset;              /* offset in activation frame */
    Symbol *plist;             /* next parameter in parameter list */
#endif
};

void free_symbols();

#endif /* Symbol_h */
