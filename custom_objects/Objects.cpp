#include "Objects.hpp"
#include <sstream>
#include <boost/regex.hpp>
#include <boost/algorithm/string/regex.hpp>

void serializeCommand(Command& cmd, std::string& out) {
	std::stringstream ss;
	boost::archive::text_oarchive oa(ss);
	oa << cmd;
	out = ss.str();
}

Command deserializeCommand(const std::string& input) {
	std::stringstream ss(input);

	boost::archive::text_iarchive ia(ss);
	Command result;
	ia >> result;
	return result;
}

bool serializeObject(CustomObject* pObj, std::string& out) {
	if (pObj == nullptr)
		return false;
	if (pObj->type() == CUSTOM_TYPE_1) {
		std::cout << "Serialize type 1" << std::endl;
		CustomObject1* pO = dynamic_cast<CustomObject1*>(pObj);
		std::stringstream ss;
		boost::archive::text_oarchive oa(ss);
		oa << *pO;
		out = ss.str();
		return true;
	}
	else if (pObj->type() == CUSTOM_TYPE_2) {
		std::cout << "Serialize type 2" << std::endl;
		CustomObject2* pO = dynamic_cast<CustomObject2*>(pObj);
		std::stringstream ss;
		boost::archive::text_oarchive oa(ss);
		oa << *pO;
		out = ss.str();
		return true;
	}
	return false;
}

std::unique_ptr<CustomObject> deserializeObject(const Command& cmd) {
	if (cmd.info.empty())
		return nullptr;

	std::unique_ptr<CustomObject> result;
	std::stringstream ss(cmd.info);
	std::cout << "before call to ia " << cmd.info.length() << cmd.info << std::endl;
	boost::archive::text_iarchive ia(ss);
	if (cmd.objType == CUSTOM_TYPE_1) {
		std::cout << "deSerialize type 1" << std::endl;
		CustomObject1* obj = new CustomObject1();
		ia >> *obj;
		result.reset(obj);
	}
	else if (cmd.objType == CUSTOM_TYPE_2) {
		std::cout << "deSerialize type 2" << std::endl;
		CustomObject2* obj = new CustomObject2();
		ia >> *obj;
		result.reset(obj);
	}
	return result;
}

void deserialize(const std::string& in, std::vector<Command>& out) {
	
	static const std::string serialization_signature = "22 serialization::archive ";
	if (in.find(serialization_signature) == std::string::npos)
		return; 

	std::vector<std::string> strs;
	boost::algorithm::split_regex(strs, in, boost::regex(serialization_signature));
	if (strs.size() > 1) {
		strs.erase(strs.begin());
	}
	else if (strs.size() == 1) {
		strs.erase(strs.begin());
		strs.push_back(in);
	}

	for (const auto& s : strs) {
		std::string obj_s(serialization_signature + s);
		try {
			Command cmd = deserializeCommand(obj_s);
			out.push_back(cmd);
		}
		catch (const std::exception & e) {
			std::cout << "Exeption " << e.what() << std::endl;
		}
	}
}