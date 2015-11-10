#include <iostream>
#include <sstream>
#include <string>
#include <fstream>
#include <vector>
#include "leveldb/db.h"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include "rapidjson/prettywriter.h"	// for stringify JSON
#include "rapidjson/filestream.h"
#include <leveldb/slice.h>
#include <sys/time.h>

using namespace std;
using namespace rapidjson;

//mac
//static string dbPath = "/home/nakshikatha/Desktop/LevelDB_CQ_Test/TestDB";


//ubuntu
static string dbPath = "/home/mohiuddin/Desktop/LevelDB_CQ_Test/DB/BenchmarDB";
static string benchmarkPath = "/home/mohiuddin/Desktop/LevelDB_CQ_Test/Benchmark/CQ1_Q75";
static string result_filePath = "/home/mohiuddin/Desktop/LevelDB_CQ_Test/Results/CQ1_Q75_result.txt";

bool SetAttr(rapidjson::Document& doc, const char* attr, long value) {
  if(!doc.IsObject() || !doc.HasMember(attr) || doc[attr].IsNull())
    return false;

  std::ostringstream pKey;
  //cout<<value << " ";
  string val =  std::to_string(value);
  //cout<<val<<endl;

	Value v;
	v.SetString(val.c_str(), val.length(), doc.GetAllocator());
	doc.RemoveMember(attr);
	doc.AddMember(attr,  v , doc.GetAllocator());
   //doc[attr].SetString(val.c_str());
   return true;

}
std::string generateJSON(std::string k, std::string v) {
    return "{\"City\": \"" + k + "\",\"State\": \"" + v + "\"}";
}

void print_vals(std::vector<string>& vals) {
  for(std::vector<string>::iterator it = vals.begin(); it != vals.end(); ++it)
    cout << "Users: " << *it << " ";
  cout<<std::endl;
}

std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    split(s, delim, elems);
    return elems;
}

std::string parse_json (std::string json)
{
	rapidjson::Document document;
	document.Parse<0>(json.c_str());

	long int sec= time(NULL);

	//cout<<sec<<endl;

	bool tmin = SetAttr(document, "Tnow", sec);

	tmin = SetAttr(document, "Tmin", sec-1000);

	if(tmin) //Query
		tmin = SetAttr(document, "Tmax", sec+1000);
	else //Event
		tmin = SetAttr(document, "Tmax", sec+5000);

	rapidjson::GenericStringBuffer<rapidjson::UTF8<>> buffer;
	rapidjson::Writer< rapidjson::GenericStringBuffer< rapidjson::UTF8<> > > writer(buffer);
	document.Accept(writer);
	std::string st=buffer.GetString();
	return st;
}

void test_frombenchmark()
{
    
    //
    //************************************************************************************
    leveldb::DB* db;
    leveldb::Options options;

    options.create_if_missing = true;
    options.using_s_index = true;
    options.primary_key = "City";
    options.secondary_key = "State";
    options.isQueryDB = false;

    ifstream ifile(benchmarkPath.c_str());
    if (!ifile) { cerr << "Can't open input file " << endl; return; }
    ofstream ofile(result_filePath.c_str(),  std::ofstream::out | std::ofstream::app );
    if (!ofile) { cerr << "Can't open output file " << endl; return; }

    std::cout << "Trying to create database\n";

    leveldb::Status status = leveldb::DB::Open(options, dbPath , &db);
    assert(status.ok());

    string line;
	int i=0;
	rapidjson::Document d;
	leveldb::ReadOptions roptions;
	double data=0, query=0;
	double durationData=0,durationQuery=0;


	vector<string> events;
	vector<string> users;

    while(getline(ifile, line)) {

    	i++;

		std::vector<std::string> x = split(line, '\t');
		leveldb::WriteOptions woptions;
		struct timeval start, end;
		if(x.size()==3) {
			leveldb::Slice key = x[1];
			if(x[1].empty())
				continue;
			//cout<<parse_json(x[2])<<endl;
			//continue;
			string json_string = parse_json(x[2]);
			leveldb::Slice json_value = json_string;
			//cout<<x[1]<<" ";
			//cout<<json_value.ToString()<<endl;

			gettimeofday(&start, NULL);
			if(x[0]=="D") {

				leveldb::Status s = db->PutC(woptions,key, json_value, users );

				gettimeofday(&end, NULL);
				durationData+= ((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec));
				//print_vals(users);
				//cout<<"Users: "<<users.size()<<endl;

				if(users.size()>0)
				{
					//cout<<"Users: "<<users.size()<<endl;
					users.clear();

				}

				data++;
			}
			else if(x[0]=="Q")
			{

				leveldb::Status s = db->GetC(roptions, key , json_value, events);
				gettimeofday(&end, NULL);
				durationQuery+= ((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec));
				//print_vals(events);

				//cout<<"Events: "<<events.size()<<endl;

				if(events.size()>0) {
					//cout<<"Events: "<<events.size()<<endl;
					events.clear();
				}
				query++;
			}
		}
//		if(i==20)
//			break;
		if (i%100000 == 0) {
			ofile<<"i: "<<i<<endl;
			ofile<<"Number of Queries: "<<query<<endl;
			ofile<<"Total Query duration: "<<durationQuery<<endl;
			ofile<<"Time per Query: "<<durationQuery/query<<endl<<endl;

			cout<<i<<endl<<"Time per Query: "<<durationQuery/query<<endl;

			ofile<<"Number of Events: "<<data<<endl;
			ofile<<"Total Event duration: "<<durationData<<endl;
			ofile<<"Time per Event: "<<durationData/data<<endl<<endl;

			cout<<"Time per Data: "<<durationData/data<<endl<<endl;

			ofile<<"Number of operation: "<<data+query<<endl;
			ofile<<"Time taken of all operation: "<<(durationData+durationQuery)<<endl;
			ofile<<"Time per operation: "<<(durationData+durationQuery)/(data+query)<<endl<<endl<<endl;
		}
    }

    delete db;

}

void testingRapidJson ()
{

	leveldb::Slice val =  "{ \"hello\" : \"world\" }";

	std::string new_key_list = "[";
	new_key_list += ("\"" + val.ToString() + "\"");
	new_key_list += "]";

	rapidjson::Document document;
	document.Parse<0>(new_key_list.c_str());

//
//	PrettyWriter<FileStream> writer(f);
//	document.Accept(writer);
	GenericStringBuffer<UTF8<>> buffer;
//	rapidjson::GenericStringBuffer<rapidjson::UTF8<>> buffer;
//
	Writer< GenericStringBuffer< UTF8<> > > writer(buffer);
	document.Accept(writer);
	std::string st=buffer.GetString();


	cout<<document.Size();
	int i = document.Size() - 1;
		  //  while (kNoOfOutputs>0&& i >= 0) {
	while ( i >= 0)
	{


		if(document[i].IsObject())
		{
			GenericStringBuffer<UTF8<>> buffer;
			Writer< GenericStringBuffer< UTF8<> > > writer(buffer);
			document[i].Accept(writer);
			std::string st=buffer.GetString();
			cout<<st;
		}

		i--;
	}


}

/*
 *
 */
int main(int argc, char** argv) {

	test_frombenchmark();
	return 0;
}

