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
		CustomObject1* pO = dynamic_cast<CustomObject1*>(pObj);
		std::stringstream ss;
		boost::archive::text_oarchive oa(ss);
		oa << *pO;
		out = ss.str();
		return true;
	}
	else if (pObj->type() == CUSTOM_TYPE_2) {
		CustomObject2* pO = dynamic_cast<CustomObject2*>(pObj);
		std::stringstream ss;
		boost::archive::text_oarchive oa(ss);
		oa << *pO;
		out = ss.str();
		return true;
	}
	return false;
}


void deserialize(const std::string& in, std::vector<Command>& out) {
	//22 serialization::archive 17 0 0 1 1 0 0
	
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
		//std::cout << obj_s << std::endl;
		
		Command cmd = deserializeCommand(obj_s);
		out.push_back(cmd);
	}
}