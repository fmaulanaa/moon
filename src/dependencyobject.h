 /*
 * dependencyobject.h: 
 *
 * Copyright 2007 Novell, Inc. (http://www.novell.com)
 *
 * See the LICENSE file included with the distribution for details.
 * 
 */

#ifndef __MONO_DEPOBJECT_H__
#define __MONO_DEPOBJECT_H__

#include "value.h"
#include "enums.h"
#include "debug.h"

#if STACK_DEBUG
#define OBJECT_TRACKING 1
#endif

#if OBJECT_TRACKING
#define GET_OBJ_ID(x) (x ? x->id : 0)
#else
#define GET_OBJ_ID(x) (-1)
#endif

class EventObject;

typedef void (*EventHandler) (EventObject *sender, gpointer calldata, gpointer closure);

struct EventList {
	int current_token;
	GSList *event_list;
};

//
// This guy provide reference counting
// and type management.
// 
// An object starts out with a reference
// count of 1 (no need to ref it after 
// creation), and will be deleted once
// the count reaches 0.
//
// DEBUGGING
// 
// first define STACK_TRACE in debug.h to 1
//
// To log all creation/destruction/ref/unref of an object,
// define OBJECT_TRACK_ID to that object's in dependencyobject.cpp.
// (this will require that you first run the program once to print
// the id of the object you're interested in).
// 
// To ensure that an object is destroyed before another object,
// you can use weak_ref/unref. For instance to ensure that child
// objects are always destroyed before their parent, make the
// child call weak_ref/unref on their parent. This won't prevent
// the parent from being destructed, but if the parent has any 
// weak refs upon destruction, a printf will be shown to the console
// (as long as OBJECT_TRACKING is defined). After that, you'll
// most likely crash when the child tries to call weak_unref on 
// its destructed parent.
//

class EventObject {
 public:	
	gint32 refcount;

#if OBJECT_TRACKING
	static int objects_created;
	static int objects_destroyed;
	static GHashTable* objects_alive;
	int id;
	GHashTable* weak_refs;

	char* GetStackTrace (const char* prefix);
	char* GetStackTrace () { return GetStackTrace (""); }
	void PrintStackTrace ();
	void Track (const char* done, const char* typname);
#define OBJECT_TRACK(x,y) Track((x),(y))
#else
#define OBJECT_TRACK(x,y)
#endif

	EventObject () : refcount(1)
	{
#if OBJECT_TRACKING
		id = ++objects_created;
		if (objects_alive == NULL)
			objects_alive = g_hash_table_new (g_direct_hash, g_direct_equal);
		g_hash_table_insert (objects_alive, this, GINT_TO_POINTER (1));
		weak_refs = NULL;
#endif
		OBJECT_TRACK ("Created", "");

		events = NULL;
	}

	virtual ~EventObject ()
	{
#if OBJECT_TRACKING
		objects_destroyed++;
		g_hash_table_remove (objects_alive, this);
		if (weak_refs) {
			int length = g_hash_table_size (weak_refs);
			if (length > 0) {
				printf ("Destroying id=%i with %i weak refs.\n", id, length);
				GList* list = g_hash_table_get_values (weak_refs);
				GList* first = list;
				while (list != NULL) {
					EventObject* eo = (EventObject*) list->data;
					printf ("\t%s %i.\n", eo->GetTypeName (), eo->id);
					list = list->next;
				}
				g_list_free (first);
			}
			g_hash_table_destroy (weak_refs);
			weak_refs = NULL;
		}
#endif
		OBJECT_TRACK ("Destroyed", "");

		FreeHandlers ();
	}

#if OBJECT_TRACKING
	void weak_ref (EventObject* base)
	{
		if (weak_refs == NULL)
			weak_refs = g_hash_table_new (g_direct_hash, g_direct_equal);
		g_hash_table_insert (weak_refs, base, base);
	}
	
	void weak_unref (EventObject* base)
	{
		// if you get a glib assertion here, you're most likely accessing an already 
		// destroyed object (or you have mismatched ref/unrefs).
		g_hash_table_remove (weak_refs, base);
	}
#endif

	void ref ()
	{
		g_atomic_int_inc (&refcount);
		OBJECT_TRACK ("Ref", GetTypeName ());
	}


	void unref ()
	{
		bool delete_me;

		delete_me = g_atomic_int_dec_and_test (&refcount);
	
		OBJECT_TRACK ("Unref", GetTypeName ());

		if (delete_me) {
			Emit (DestroyedEvent);

			delete this;
		}
	}

	//
	// Is:
	//    Similar to C#'s is: it checks if this object is of this kind or 
	//    a derived class.
	
	bool Is (Type::Kind k)
	{
		return GetType ()->IsSubclassOf (k);
	}

	Type *GetType ()
	{
		return Type::Find (GetObjectType ());
	}
	
	char *GetTypeName ()
	{
		return Type::Find (GetObjectType ())->name;
	}

	int AddHandler (const char *event_name, EventHandler handler, gpointer data);
	void RemoveHandler (const char *event_name, EventHandler handler, gpointer data);
	void RemoveHandler (const char *event_name, int token);

	int AddHandler (int event_id, EventHandler handler, gpointer data);
	void RemoveHandler (int event_id, EventHandler handler, gpointer data);
	void RemoveHandler (int event_id, int token);

	virtual Type::Kind GetObjectType () { return Type::EVENTOBJECT; }

	static int DestroyedEvent;

 protected:
	void Emit (char *event_name, gpointer calldata = NULL);
	void Emit (int event_id, gpointer calldata = NULL);

 private:
	void FreeHandlers ();

	EventList *events;
};


class DependencyObject : public EventObject {
 public:

	DependencyObject ();
	virtual ~DependencyObject ();
	static DependencyProperty *Register (Type::Kind type, const char *name, Value *default_value);
	static DependencyProperty *Register (Type::Kind type, const char *name, Type::Kind vtype);
	static DependencyProperty *Register (Type::Kind type, const char *name, Value *default_value, Type::Kind vtype);
	static DependencyProperty *RegisterNullable (Type::Kind type, const char *name, Type::Kind vtype);
	static DependencyProperty *RegisterFull (Type::Kind type, const char *name, Value *default_value, Type::Kind vtype, bool attached, bool readonly);
	
	static DependencyProperty *GetDependencyProperty (Type::Kind type, const char *name);
	static DependencyProperty *GetDependencyProperty (Type::Kind type, const char *name, bool inherits);
	static DependencyProperty *NameProperty;
	virtual void SetValue (DependencyProperty *property, Value *value);
	virtual void SetValue (DependencyProperty *property, Value value);
	void SetValue (const char *name, Value *value);
	void SetValue (const char *name, Value value);
	virtual Value *GetValue (DependencyProperty *property);
	Value *GetValueNoDefault (DependencyProperty *property);
	Value *GetValue (const char *name);
	void ClearValue (DependencyProperty *property);
	bool HasProperty (const char *name, bool inherits);
	DependencyProperty *GetDependencyProperty (const char *name);
	DependencyObject *FindName (const char *name);
	NameScope *FindNameScope ();

	static GHashTable *properties;

	virtual void OnPropertyChanged (DependencyProperty *property);
	virtual void OnSubPropertyChanged (DependencyProperty *prop, DependencyObject *obj, DependencyProperty *subprop) { }

	//
	// OnChildPropertyChanged:
	//    This is raised on objects when a child of this object has had one of its
	//    properties changed.   This is used so that owning objects can monitor if
	//    one of the attached properties in a child must be acted upon
	//
	//    This code will go up in the ownership chain until this is handled, by 
	//    returning TRUE.
	//
	virtual bool OnChildPropertyChanged (DependencyProperty *prop, DependencyObject *child) { return FALSE; }


	//
	// OnCollectionChanged:
	//
	virtual void OnCollectionChanged (Collection *col, CollectionChangeType type, DependencyObject *obj, DependencyProperty *prop) { }

	virtual Type::Kind GetObjectType ();

	const char *GetName ()
	{
		Value *v = GetValue (DependencyObject::NameProperty);
		return v ? v->AsString () : NULL;
	}
	
	void SetLogicalParent (DependencyObject *logical_parent);
	DependencyObject* GetLogicalParent ();

	static void Shutdown ();

	void Attach (DependencyProperty *property, DependencyObject *container);
	void Detach (DependencyProperty *property, DependencyObject *container);
	
	virtual Surface *GetSurface () { return NULL; }
	virtual void SetSurface (Surface *surface) { } // Do nothing here.

 protected:
	void NotifyAttachersOfPropertyChange (DependencyProperty *property);
	void NotifyParentOfPropertyChange (DependencyProperty *property, bool only_exact_type);

	void DetachAll ();

 private:
	GHashTable        *current_values;
	GSList            *attached_list;
	DependencyObject  *logical_parent;
};


//
// DependencyProperty
//
class DependencyProperty {
 public:
	DependencyProperty () {} ;
	~DependencyProperty ();
	DependencyProperty (Type::Kind type, const char *name, Value *default_value, Type::Kind value_type, bool attached, bool readonly);

	char *name;
	Value *default_value;
	Type::Kind type;
	bool is_attached_property;
	Type::Kind value_type;
	bool is_nullable;

	bool IsNullable () { return is_nullable; }
	bool IsReadOnly () { return is_readonly; }

 private:
	bool is_readonly;
};

G_BEGIN_DECLS

void base_ref (EventObject *obj);
void base_unref (EventObject *obj);
void base_unref_delayed (EventObject *obj);

void drain_unrefs ();

Value *dependency_object_get_value (DependencyObject *object, DependencyProperty *prop);
Value *dependency_object_get_value_no_default (DependencyObject *object, DependencyProperty *prop);
void   dependency_object_set_value (DependencyObject *object, DependencyProperty *prop, Value *val);

DependencyObject *dependency_object_find_name (DependencyObject *obj, const char *name, Type::Kind *element_type);
const char       *dependency_object_get_name  (DependencyObject *obj);
void dependency_object_set_name (DependencyObject *obj, const char *name);

Type::Kind dependency_object_get_object_type (DependencyObject *obj);
const char *dependency_object_get_type_name (DependencyObject *obj);

DependencyProperty *dependency_property_lookup (Type::Kind type, char *name);
char *dependency_property_get_name (DependencyProperty* property);
bool  dependency_property_is_nullable (DependencyProperty* property);
Type::Kind dependency_property_get_value_type (DependencyProperty* property);
DependencyProperty *resolve_property_path (DependencyObject **o, const char *path);

void dependencyobject_init (void);


void event_object_add_event_handler (EventObject *o, const char *event, EventHandler handler, gpointer closure);
void event_object_remove_event_handler (EventObject *o, const char *event, EventHandler handler, gpointer closure);

G_END_DECLS

#endif /* __MONO_DEPOBJECT_H__ */

