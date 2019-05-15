/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#pragma once

enum SerializeFormat
{
	/** Shown to a human (does not need to be unserializable) */
	FORMAT_USER,
	/** Passed internally to this process (i.e. for /RELOADMODULE) */
	FORMAT_INTERNAL,
	/** Passed to other servers on the network (i.e. METADATA s2s command) */
	FORMAT_NETWORK,
	/** Stored on disk (i.e. permchannel database) */
	FORMAT_PERSIST
};

/** Class represnting an extension of some object
 */
class CoreExport ExtensionItem : public ServiceProvider, public usecountbase
{
 public:
	/** Extensible subclasses
	 */
	enum ExtensibleType
	{
		EXT_USER,
		EXT_CHANNEL,
		EXT_MEMBERSHIP
	};

	/** Type (subclass) of Extensible that this ExtensionItem is valid for
	 */
	const ExtensibleType type;

	ExtensionItem(const std::string& key, ExtensibleType exttype, Module* owner);
	virtual ~ExtensionItem();
	/** Serialize this item into a string
	 *
	 * @param format The format to serialize to
	 * @param container The object containing this item
	 * @param item The item itself
	 */
	virtual std::string serialize(SerializeFormat format, const Extensible* container, void* item) const = 0;
	/** Convert the string form back into an item
	 * @param format The format to serialize from (not FORMAT_USER)
	 * @param container The object that this item applies to
	 * @param value The return from a serialize() call that was run elsewhere with this key
	 */
	virtual void unserialize(SerializeFormat format, Extensible* container, const std::string& value) = 0;
	/** Free the item */
	virtual void free(Extensible* container, void* item) = 0;

	/** Register this object in the ExtensionManager
	 */
	void RegisterService() CXX11_OVERRIDE;

 protected:
	/** Get the item from the internal map */
	void* get_raw(const Extensible* container) const;
	/** Set the item in the internal map; returns old value */
	void* set_raw(Extensible* container, void* value);
	/** Remove the item from the internal map; returns old value */
	void* unset_raw(Extensible* container);
};

/** class Extensible is the parent class of many classes such as User and Channel.
 * class Extensible implements a system which allows modules to 'extend' the class by attaching data within
 * a map associated with the object. In this way modules can store their own custom information within user
 * objects, channel objects and server objects, without breaking other modules (this is more sensible than using
 * a flags variable, and each module defining bits within the flag as 'theirs' as it is less prone to conflict and
 * supports arbitary data storage).
 */
class CoreExport Extensible : public classbase
{
 public:
	typedef insp::flat_map<reference<ExtensionItem>, void*> ExtensibleStore;

	// Friend access for the protected getter/setter
	friend class ExtensionItem;
 private:
	/** Private data store.
	 * Holds all extensible metadata for the class.
	 */
	ExtensibleStore extensions;

	/** True if this Extensible has been culled.
	 * A warning is generated if false on destruction.
	 */
	unsigned int culled:1;
 public:
	/**
	 * Get the extension items for iteraton (i.e. for metadata sync during netburst)
	 */
	inline const ExtensibleStore& GetExtList() const { return extensions; }

	Extensible();
	CullResult cull() CXX11_OVERRIDE;
	virtual ~Extensible();
	void doUnhookExtensions(const std::vector<reference<ExtensionItem> >& toRemove);

	/**
	 * Free all extension items attached to this Extensible
	 */
	void FreeAllExtItems();
};

class CoreExport ExtensionManager
{
 public:
	typedef std::map<std::string, reference<ExtensionItem> > ExtMap;

	bool Register(ExtensionItem* item);
	void BeginUnregister(Module* module, std::vector<reference<ExtensionItem> >& list);
	ExtensionItem* GetItem(const std::string& name);

	/** Get all registered extensions keyed by their names
	 * @return Const map of ExtensionItem pointers keyed by their names
	 */
	const ExtMap& GetExts() const { return types; }

 private:
	ExtMap types;
};

/** Base class for items that are NOT synchronized between servers */
class CoreExport LocalExtItem : public ExtensionItem
{
 public:
	LocalExtItem(const std::string& key, ExtensibleType exttype, Module* owner);
	virtual ~LocalExtItem();
	std::string serialize(SerializeFormat format, const Extensible* container, void* item) const CXX11_OVERRIDE;
	void unserialize(SerializeFormat format, Extensible* container, const std::string& value) CXX11_OVERRIDE;
	void free(Extensible* container, void* item) CXX11_OVERRIDE = 0;
};

namespace Ext
{
	template<typename T>
	struct BaseInserter
	{
		typedef T ContainerType;
		typedef typename ContainerType::value_type value_type;

		virtual void insert(ContainerType& container, const value_type& value) const = 0;
	};

	template<typename T>
	struct Inserter
		: BaseInserter<T>
	{
		void insert(T& container, const typename T::value_type& value) const CXX11_OVERRIDE
		{
			container.push_back(value);
		}
	};

	template<typename T, typename U, typename C, typename E>
	struct Inserter<insp::flat_map<T, U, C, E> >
		: BaseInserter<insp::flat_map<T, U, C, E> >
	{
		typedef typename BaseInserter<insp::flat_map<T, U, C, E> >::ContainerType ContainerType;
		typedef typename ContainerType::value_type value_type;

		void insert(ContainerType& container, const value_type& value) const CXX11_OVERRIDE
		{
			container.insert(value);
		}
	};

	template<typename T, typename C, typename E>
	struct Inserter<insp::flat_set<T, C, E> >
		: BaseInserter<insp::flat_set<T, C, E> >
	{
		typedef typename BaseInserter<insp::flat_set<T, C, E> >::ContainerType ContainerType;
		typedef typename ContainerType::value_type value_type;

		void insert(ContainerType& container, const value_type& value) const CXX11_OVERRIDE
		{
			container.insert(value);
		}
	};

	std::ostream& EscapeNulls(const std::string& s, std::ostream& os);

	typedef std::vector<std::string> StringList;

	StringList SplitUnescapeNulls(const std::string& str);

	template<typename T>
	struct SerializeBase
	{
		typedef T value_type;

		virtual void serialize(SerializeFormat format, const T& value, const Extensible* container, const ExtensionItem* extItem, std::ostream& os) const = 0;

		std::string serializeStr(SerializeFormat format, const T& value, const Extensible* container, const ExtensionItem* extItem) const
		{
			std::ostringstream sstr;
			this->serialize(format, value, container, extItem, sstr);
			return sstr.str();
		}

		virtual T* unserialize(SerializeFormat format, const std::string& value, const Extensible* container, const ExtensionItem* extItem) const = 0;
	};

	template<typename T>
	struct Serialize
		: SerializeBase<T>
	{
		void serialize(SerializeFormat format, const T& value, const Extensible* container, const ExtensionItem* extItem, std::ostream& os) const CXX11_OVERRIDE
		{
			value.Serialize(format, container, extItem, os);
		}

		T* unserialize(SerializeFormat format, const std::string& value, const Extensible* container, const ExtensionItem* extItem) const CXX11_OVERRIDE
		{
			return T::FromString(format, value, container, extItem);
		}
	};

	template<>
	struct Serialize<std::string>
		: SerializeBase<std::string>
	{
		void serialize(SerializeFormat format, const value_type& value, const Extensible* container, const ExtensionItem* extItem, std::ostream& os) const CXX11_OVERRIDE
		{
			os << value;
		}

		value_type* unserialize(SerializeFormat format, const std::string& value, const Extensible* container, const ExtensionItem* extItem) const CXX11_OVERRIDE
		{
			return new value_type(value);
		}
	};

	template<>
	struct Serialize<User*>
		: SerializeBase<User*>
	{
		// TODO implement
		void serialize(SerializeFormat format, const value_type& value, const Extensible* container, const ExtensionItem* extItem, std::ostream& os) const CXX11_OVERRIDE
		{
		}

		value_type* unserialize(SerializeFormat format, const std::string& value, const Extensible* container, const ExtensionItem* extItem) const CXX11_OVERRIDE
		{
			return NULL;
		}
	};

	template<>
	struct Serialize<LocalUser*>
		: SerializeBase<LocalUser*>
	{
		// TODO implement
		void serialize(SerializeFormat format, const value_type& value, const Extensible* container, const ExtensionItem* extItem, std::ostream& os) const CXX11_OVERRIDE
		{
		}

		value_type* unserialize(SerializeFormat format, const std::string& value, const Extensible* container, const ExtensionItem* extItem) const CXX11_OVERRIDE
		{
			return NULL;
		}
	};

	template<typename T>
	struct SerializeContainer
		: SerializeBase<T>
	{
		typedef typename T::const_iterator iter;
		typedef typename iter::value_type value_type;

		const Serialize<value_type> ser;
		const Inserter<T> inserter;

		void serialize(SerializeFormat format, const T& value, const Extensible* container, const ExtensionItem* extItem, std::ostream& os) const CXX11_OVERRIDE
		{
			for (iter it = value.begin(); it != value.end(); ++it)
				EscapeNulls(ser.serializeStr(format, *it, container, extItem), os) << '\0';
		}

		T* unserialize(SerializeFormat format, const std::string& value, const Extensible* container, const ExtensionItem* extItem) const CXX11_OVERRIDE
		{
			StringList values = SplitUnescapeNulls(value);
			T* cont = new T;

			for (StringList::const_iterator it = values.begin(), it_end = values.end(); it != it_end; ++it)
			{
				value_type* o = ser.unserialize(format, *it, container, extItem);
				if (o)
					inserter.insert(*cont, *o);

				delete o;
			}

			return cont;
		}
	};

	template<typename T, typename A>
	struct Serialize<std::vector<T, A> >
		: SerializeContainer<std::vector<T, A> >
	{
	};

	template<typename T, typename A>
	struct Serialize<std::deque<T, A> >
		: SerializeContainer<std::deque<T, A> >
	{
	};

	template<typename T, typename Comp, typename ElementComp>
	struct Serialize<insp::flat_set<T, Comp, ElementComp> >
		: SerializeContainer<insp::flat_set<T, Comp, ElementComp> >
	{
	};

	template<typename T, typename Comp, typename ElementComp>
	struct Serialize<insp::flat_multiset<T, Comp, ElementComp> >
		: SerializeContainer<insp::flat_multiset<T, Comp, ElementComp> >
	{
	};

	template<typename T, typename V, typename Comp, typename ElementComp>
	struct Serialize<insp::flat_map<T, V, Comp, ElementComp> >
		: SerializeContainer<insp::flat_map<T, V, Comp, ElementComp> >
	{
	};

	template<typename T, typename V, typename Comp, typename ElementComp>
	struct Serialize<insp::flat_multimap<T, V, Comp, ElementComp> >
		: SerializeContainer<insp::flat_multimap<T, V, Comp, ElementComp> >
	{
	};

	template<typename T1, typename T2>
	struct Serialize<std::pair<T1, T2> >
		: SerializeBase<std::pair<T1, T2> >
	{
		Serialize<T1> ser1;
		Serialize<T2> ser2;

		typedef typename SerializeBase<std::pair<T1, T2> >::value_type value_type;

		void serialize(SerializeFormat format, const value_type& value, const Extensible* container, const ExtensionItem* extItem, std::ostream& os) const CXX11_OVERRIDE
		{
			EscapeNulls(ser1.serializeStr(format, value.first, container, extItem), os) << '\0';

			EscapeNulls(ser2.serializeStr(format, value.second, container, extItem), os) << '\0';
		}

		value_type* unserialize(SerializeFormat format, const std::string& value, const Extensible* container, const ExtensionItem* extItem) const CXX11_OVERRIDE
		{
			if (value.empty())
				return NULL;

			StringList values = SplitUnescapeNulls(value);

			T1* v1 = ser1.unserialize(format, values.at(0), container, extItem);
			T2* v2 = ser2.unserialize(format, values.at(1), container, extItem);

			if (!v1 || !v2)
				return NULL;

			value_type* vt = new value_type(*v1, *v2);

			delete v1;
			delete v2;

			return vt;
		}
	};

	/**
	 * Allows for basic serialization of simple structs that contain only primitive types
	 * Example:
	 * 		struct Data
	 * 		{
	 * 			int a;
	 *			unsigned int b;
	 *		};
	 */
	template<typename T>
	struct SerializePrimitive
		: SerializeBase<T>
	{
		void serialize(SerializeFormat format, const T& value, const Extensible* container, const ExtensionItem* extItem, std::ostream& os) const CXX11_OVERRIDE
		{
			os.write((const char*)&value, sizeof(T));
		}

		T* unserialize(SerializeFormat format, const std::string& value, const Extensible* container, const ExtensionItem* extItem) const CXX11_OVERRIDE
		{
			return new T(*(const T*)value.c_str());
		}
	};

	template<typename T>
	struct SerializeNumeric
		: SerializePrimitive<T>
	{
		void serialize(SerializeFormat format, const T& value, const Extensible* container, const ExtensionItem* extItem, std::ostream& os) const CXX11_OVERRIDE
		{
			if (format == FORMAT_USER)
				os << ConvNumeric(value);
			else
				SerializePrimitive<T>::serialize(format, value, container, extItem, os);
		}

		T* unserialize(SerializeFormat format, const std::string& value, const Extensible* container, const ExtensionItem* extItem) const CXX11_OVERRIDE
		{
			if (format == FORMAT_USER)
				return new T(ConvToNum<T>(value));

			return SerializePrimitive<T>::unserialize(format, value, container, extItem);
		}
	};

	template<>
	struct Serialize<int8_t>
		: SerializeNumeric<int8_t>
	{
	};

	template<>
	struct Serialize<int16_t>
		: SerializeNumeric<int16_t>
	{
	};

	template<>
	struct Serialize<int32_t>
		: SerializeNumeric<int32_t>
	{
	};

	template<>
	struct Serialize<int64_t>
		: SerializeNumeric<int64_t>
	{
	};

	template<>
	struct Serialize<uint8_t>
		: SerializeNumeric<uint8_t>
	{
	};

	template<>
	struct Serialize<uint16_t>
		: SerializeNumeric<uint16_t>
	{
	};

	template<>
	struct Serialize<uint32_t>
		: SerializeNumeric<uint32_t>
	{
	};

	template<>
	struct Serialize<uint64_t>
		: SerializeNumeric<uint64_t>
	{
	};

	template<>
	struct Serialize<bool>
		: SerializeNumeric<bool>
	{
		void serialize(SerializeFormat format, const bool& value, const Extensible* container, const ExtensionItem* extItem, std::ostream& os) const CXX11_OVERRIDE
		{
			if (format == FORMAT_USER)
				os << (value ? "true" : "false");
			else
				SerializeNumeric<bool>::serialize(format, value, container, extItem, os);
		}
	};
}

template <typename T, typename Del = stdalgo::defaultdeleter<T> >
class UnserializableSimpleExtItem
	: public LocalExtItem
{
 public:
	UnserializableSimpleExtItem(const std::string& Key, ExtensibleType exttype, Module* parent)
		: LocalExtItem(Key, exttype, parent)
	{
	}

	virtual ~UnserializableSimpleExtItem()
	{
	}

	inline T* get(const Extensible* container) const
	{
		return static_cast<T*>(get_raw(container));
	}

	inline void set(Extensible* container, const T& value)
	{
		T* ptr = new T(value);
		T* old = static_cast<T*>(set_raw(container, ptr));
		Del del;
		del(old);
	}

	inline void set(Extensible* container, T* value)
	{
		T* old = static_cast<T*>(set_raw(container, value));
		Del del;
		del(old);
	}

	inline void unset(Extensible* container)
	{
		T* old = static_cast<T*>(unset_raw(container));
		Del del;
		del(old);
	}

	void free(Extensible* container, void* item) CXX11_OVERRIDE
	{
		Del del;
		del(static_cast<T*>(item));
	}
};

template<typename T, typename Del = stdalgo::defaultdeleter<T> >
class SimpleExtItem : public UnserializableSimpleExtItem<T, Del>
{
	const Ext::Serialize<T> serializer;

 public:
	SimpleExtItem(const std::string& Key, ExtensionItem::ExtensibleType exttype, Module* parent)
		: UnserializableSimpleExtItem<T, Del>(Key, exttype, parent)
	{
	}

	std::string serialize(SerializeFormat format, const Extensible* container, void* item) const CXX11_OVERRIDE
	{
		if (!item || format == FORMAT_NETWORK)
			return "";

		return serializer.serializeStr(format, *static_cast<T*>(item), container, this);
	}

	void unserialize(SerializeFormat format, Extensible* container, const std::string& value) CXX11_OVERRIDE
	{
		if (format == FORMAT_NETWORK)
			return;

		T* t = serializer.unserialize(format, value, container, this);
		if (t)
			this->set(container, t);
	}
};

class CoreExport LocalStringExt : public SimpleExtItem<std::string>
{
 public:
	LocalStringExt(const std::string& key, ExtensibleType exttype, Module* owner);
	virtual ~LocalStringExt();
};

class CoreExport LocalIntExt : public LocalExtItem
{
 public:
	LocalIntExt(const std::string& key, ExtensibleType exttype, Module* owner);
	virtual ~LocalIntExt();
	std::string serialize(SerializeFormat format, const Extensible* container, void* item) const CXX11_OVERRIDE;
	void unserialize(SerializeFormat format, Extensible* container, const std::string& value) CXX11_OVERRIDE;
	intptr_t get(const Extensible* container) const;
	intptr_t set(Extensible* container, intptr_t value);
	void unset(Extensible* container) { set(container, 0); }
	void free(Extensible* container, void* item) CXX11_OVERRIDE;
};

class CoreExport StringExtItem : public ExtensionItem
{
 public:
	StringExtItem(const std::string& key, ExtensibleType exttype, Module* owner);
	virtual ~StringExtItem();
	std::string* get(const Extensible* container) const;
	std::string serialize(SerializeFormat format, const Extensible* container, void* item) const CXX11_OVERRIDE;
	void unserialize(SerializeFormat format, Extensible* container, const std::string& value) CXX11_OVERRIDE;
	void set(Extensible* container, const std::string& value);
	void unset(Extensible* container);
	void free(Extensible* container, void* item) CXX11_OVERRIDE;
};
