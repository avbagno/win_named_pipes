#pragma once

#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/serialization.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>

#include <string>
#include <sstream>
#include <iostream>

enum CustomObjectsType  {
	CUSTOM_TYPE_1,
	CUSTOM_TYPE_2
};

enum CommandType {
	CREATE_OBJECT,
	GET_OBJECT,
	GET_OBJECT_MEMBER,
	
	ACK_OK,
	ACK_FAIL
};

struct Command {
	CommandType cmd;

	CustomObjectsType objType;
	int objId;
	std::string info;
	//friend class boost::serialization::access;

	template<class Archive>
	void serialize(Archive& ar, const unsigned int version)
	{
		ar& cmd;
		ar& objType;
		ar& objId;
		ar& info;
	}
};

void serializeCommand(Command& cmd, std::string& out); 
Command deserializeCommand(const std::string& in);

class CustomObject {
public:
	virtual ~CustomObject() {}
	virtual CustomObjectsType type() = 0;
	virtual bool exec(const std::string& cmd, std::string& out) = 0;
};

bool serializeObject(CustomObject* ptr, std::string& out);
class CustomObject1 : public CustomObject
{
private:
	friend class boost::serialization::access;

	template<class Archive>
	void serialize(Archive& ar, const unsigned int version)
	{
		ar& _m2;
		ar& _m1;
	}

	int _m1;
	float _m2;
public:
	bool exec(const std::string& cmd, std::string& out) override {
		std::stringstream ss;
		if (cmd == "getM1") {
			ss << getM1();
		} else if (cmd == "getM2") {
			ss << getM2();
		}
		else if (cmd == "_m1") {
			ss << _m1;
		}
		else if (cmd == "_m2") {
			ss << _m2;
		}
		out = ss.str();
		return !out.empty();
	}
	int getM1() { return _m1; };
	float getM2() { return _m2; };
	CustomObjectsType type() override { return CustomObjectsType::CUSTOM_TYPE_1; }
};

class CustomObject2 : public CustomObject
{
private:
	friend class boost::serialization::access;

	template<class Archive>
	void serialize(Archive& ar, const unsigned int version)
	{
		ar& _m3;
		ar& _m4;
		ar& _m5;
	}
	int _m3;
	int _m4;
	float _m5;
public:
	bool exec(const std::string& cmd, std::string& out) override {
		return false;
	}
	int getM3() { return _m3; };
	int getM4() { return _m4; };
	float getM5() { return _m5; };
	CustomObjectsType type() override { return CustomObjectsType::CUSTOM_TYPE_2; }
};