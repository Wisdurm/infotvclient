#include "json.h"
#include <stdexcept>
#include <string>
#include <exception>
#include <vector>

struct JsonObjectElementWrapper {
	json_object_element_s* element;

	std::string name() const
	{
		json_string_s* v = element->name;
		if (not v)
			throw std::bad_cast();
		std::string str = v->string;
		return str;
	}

	json_value_s* value() const
	{
		return element->value;
	}
};

struct JsonObjectWrapper {
	json_object_s* object;

	JsonObjectElementWrapper operator[] (int index) const
	{
		json_object_element_s* it = object->start;
		for (int i = 0; i < index; i++) {
			it = it->next;
		}
		if (not it)
			throw std::out_of_range("Wrong index lol");
		return {it};

	}
};

struct JsonValueWrapper {
	json_value_s* value;

	explicit operator std::string() const
	{
		json_string_s* v = json_value_as_string(value);
		if (not v)
			throw std::bad_cast();
		std::string str = v->string;
		return str;
	}

	explicit operator int() const
	{
		json_number_s* v = json_value_as_number(value);
		if (not v)
			throw std::bad_cast();
		return std::stoi(v->number);
	}

	explicit operator bool() const
	{
		if (json_value_is_true(value))
			return true;
		else if (json_value_is_false(value))
			return false;
		throw std::bad_cast();
	}

	explicit operator JsonObjectWrapper() const
	{
		json_object_s* v = json_value_as_object(value);
		if (not v)
			throw std::bad_cast();
		return {v};
	}

	explicit operator std::vector<JsonValueWrapper>() const
	{
		json_array_s* v = json_value_as_array(value);
		if (not v)
			throw std::bad_cast();
		std::vector<JsonValueWrapper> elems;
		auto it = v->start;
		while (it) {
			elems.push_back(JsonValueWrapper(it->value));
			it = it->next;
		}
		return elems;
	}
};

struct JsonWrapper {
	json_value_s* root;

	JsonWrapper(std::string const& str)
	{
		root = json_parse(str.c_str(), str.size());
	}

	~JsonWrapper()
	{
		free(root);
	}

	JsonObjectWrapper object() const
	{
		return JsonObjectWrapper(json_value_as_object(root));
	}
};
