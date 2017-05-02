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
#include <leveldb/cache.h>
#include <sys/time.h>
#include <thread>
#include <mutex>

using namespace std;
using namespace rapidjson;

//server
//static string dbPath = "/home/mabdu002/ContinuousQuery/DB/BenchmarkDB";
//static string benchmarkPath = "/home/mabdu002/ContinuousQuery/Benchmark/RD_CQ1_Q50";
//static string result_filePath = "/home/mabdu002/ContinuousQuery/Results/Baseline_RD_Q50_result.csv";


//ubuntu
static string dbPath = "/home/mohiuddin/Desktop/LevelDB_CQ_Test/DB/BenchmarkDB";
static string benchmarkPath = "/home/mohiuddin/Desktop/LevelDB_CQ_Test/Benchmark/RD_CQ1_Q50";
static string result_filePath = "/home/mohiuddin/Desktop/LevelDB_CQ_Test/Results/Baseline_Complex_RD_Q50_all_result.csv";

std::mutex m;
double durationQuery = 0 , query = 0, eventsc_rep = 0;
//double query, durationQ;

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

std::string parse_json (std::string json, long int &sec)
{
	rapidjson::Document document;
	document.Parse<0>(json.c_str());

	sec= time(NULL);

	//cout<<sec<<endl;

	bool tmin = SetAttr(document, "Tnow", sec);

	tmin = SetAttr(document, "Tmin", sec-10);

	if(tmin) //Query
		tmin = SetAttr(document, "Tmax", sec+10);
	else //Event
		tmin = SetAttr(document, "Tmax", sec+15);

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

    //long sizeperblock = 8*1024;
    size_t cachesize =  100L * 1000L * 1000L * 1024L; //100MB

    //options.block_size = sizeperblock;

    options.block_cache = leveldb::NewLRUCache(cachesize);

    options.Q_block_cache = leveldb::NewLRUCache(cachesize);

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
	long userc = 0, eventc = 0;

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
			long int sec;
			string json_string = parse_json(x[2],sec);
			leveldb::Slice json_value = json_string;
			//cout<<x[1]<<" ";
			//cout<<json_value.ToString()<<endl;

			gettimeofday(&start, NULL);
			if(x[0]=="D") {

				leveldb::Status s = db->PutBaseComplexQuery(woptions,key, json_value, users, events );
				//leveldb::Status s = db->PutC(woptions, key , json_value, users);

				gettimeofday(&end, NULL);
				durationData+= ((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec));
				//print_vals(users);
				//cout<<"Users: "<<users.size()<<endl;

				if(users.size()>0)
				{
					//cout<<"Users: "<<users.size()<<endl;
					userc+=users.size();
					users.clear();

				}
				if(events.size()>0)
				{
					//cout<<"Events: "<<events.size()<<endl;
					events.clear();
				}

				data++;
			}
			else if(x[0]=="Q")
			{

				//leveldb::Status s = db->GetC(roptions, key , json_value, events);
				leveldb::Status s = db->GetBaseComplexQuery(roptions, key , json_value, users, events );

				gettimeofday(&end, NULL);
				durationQuery+= ((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec));
				//print_vals(events);

				//cout<<"Events: "<<events.size()<<endl;
				if(users.size()>0)
				{
					//cout<<"Users: "<<users.size()<<endl;
					users.clear();

				}
				if(events.size()>0) {
					//cout<<"Events: "<<events.size()<<endl;
					eventc+=events.size();
					events.clear();
				}
				query++;
			}
		}
//		if(i==20)
//			break;
		if (i%50000 == 0) {
//			ofile<<"i: "<<i<<endl;
//			ofile<<"Number of Queries: "<<query<<endl;
//			ofile<<"Total Query duration: "<<durationQuery<<endl;
//			ofile<<"Time per Query: "<<durationQuery/query<<endl<<endl;
//
			//cout<<i<<endl<<"Time per Query: "<<durationQuery/query<<endl;
//
//			ofile<<"Number of Events: "<<data<<endl;
//			ofile<<"Total Event duration: "<<durationData<<endl;
//			ofile<<"Time per Event: "<<durationData/data<<endl<<endl;
//
			//cout<<"Time per Data: "<<durationData/data<<endl<<endl;
//
//			ofile<<"Number of operation: "<<data+query<<endl;
//			ofile<<"Time taken of all operation: "<<(durationData+durationQuery)<<endl;
//			ofile<<"Time per operation: "<<(durationData+durationQuery)/(data+query)<<endl<<endl<<endl;

			//ofile<< i <<"," << (durationData+durationQuery)/i <<"," << query <<"," << durationQuery/query <<"," << data <<"," << durationData/data <<", hit-> " << db->hit <<", miss-> " << db->miss <<endl;
			ofile<< i <<"," << (durationData+durationQuery)/i <<"," << query <<"," << durationQuery/query <<"," << data <<"," << durationData/data << ","<< userc/data <<","<<eventc/query<<endl;

		}
    }

    delete db;
    delete options.block_cache;
    delete options.Q_block_cache;
}

void testingRapidJson ()
{

	leveldb::Slice val =  "{ \"hello\" : \"world\" }";

	std::string new_key_list = "[";
	new_key_list += ("\"" + val.ToString() + "\"");
	new_key_list += "]";

	rapidjson::Document document;
	document.Parse<0>(new_key_list.c_str());

//perform_repetative_query
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


void perform_repetative_query(std::string key, std::string  value, leveldb::DB* db, long int sec  )
{
	rapidjson::Document document;
	document.Parse<0>(value.data());
	struct timeval start, end;

	double durationQ = 0;
	//	sec= time(NULL);

	//cout<<sec<<endl;

	long int tmin = sec;
	long int tnow = time(NULL);
	for(int i = 0; i<= 10 ;i++)
	{

		SetAttr(document, "Tnow", tnow);

		tmin = SetAttr(document, "Tmin", tmin);



//		if(tmin) //Query
//			tmin = SetAttr(document, "Tmax", sec+1000);
//		else //Event
//			tmin = SetAttr(document, "Tmax", sec+1500);

		rapidjson::GenericStringBuffer<rapidjson::UTF8<>> buffer;
		rapidjson::Writer< rapidjson::GenericStringBuffer< rapidjson::UTF8<> > > writer(buffer);
		document.Accept(writer);
		std::string st=buffer.GetString();

		leveldb::Slice keys = key;
		leveldb::Slice sts = st;

		vector<string> events;
		m.lock();
		gettimeofday(&start, NULL);
		db->GetR(leveldb::ReadOptions(), keys , sts, events );
		gettimeofday(&end, NULL);
		eventsc_rep+=events.size();
		m.unlock();
		durationQ+= ((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec));

		//events.clear();
		std::this_thread::sleep_for(std::chrono::seconds(1));

		tmin = tnow;
		tnow = time(NULL);

	}

	{
		m.lock();
		durationQuery+=durationQ;
		query++;
		m.unlock();
	}

}



void test_from_benchmark_repetative()
{

	dbPath = "/home/mohiuddin/Desktop/LevelDB_CQ_Test/DB/BenchmarkDB_Passive";
	benchmarkPath = "/home/mohiuddin/Desktop/LevelDB_CQ_Test/Benchmark/RD_CQ1_Q75";
	result_filePath = "/home/mohiuddin/Desktop/LevelDB_CQ_Test/Results/Baseline_PassiveData_Q75_result.csv";
    leveldb::DB* db;
    leveldb::Options options;

    //long sizeperblock = 8*1024;
    size_t cachesize =  100L * 1000L * 1000L * 1024L; //100MB

    //options.block_size = sizeperblock;

    options.block_cache = leveldb::NewLRUCache(cachesize);

    //options.Q_block_cache = leveldb::NewLRUCache(cachesize);

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
	double data=0; //query=0;
	double durationData=0;//durationQuery=0;


	vector<string> events;
	vector<string> users;

	std::vector<std::thread> threadlist;
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
			long int sec;
			string json_string = parse_json(x[2], sec);
			leveldb::Slice json_value = json_string;
			//cout<<x[1]<<" ";
			//cout<<json_value.ToString()<<endl;


			if(x[0]=="D") {

				m.lock();
				gettimeofday(&start, NULL);
				leveldb::Status s = db->PutR(woptions, key, json_value);
				gettimeofday(&end, NULL);
				m.unlock();


				durationData+= ((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec));
				//print_vals(users);
				//cout<<"Users: "<<users.size()<<endl;

				data++;
			}
			else if(x[0]=="Q")
			{

				//leveldb::Status s = db->GetC(roptions, key , json_value, events);

				threadlist.push_back(std::thread (perform_repetative_query, x[1], json_string, db, sec));
				//std::thread doquery(testRep , 1, "ss", db, &query);
				//doquery.join();
				//leveldb::Status s = db->GetR(roptions, key , json_value, events );

//				gettimeofday(&end, NULL);
//				durationQuery+= ((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec));
				//print_vals(events);

//
//				if(events.size()>0) {
//					//cout<<"Events: "<<events.size()<<endl;
//					events.clear();
//				}
//				query++;
			}
		}
//		if(i==20)
//			break;
		if (i%10000 == 0) {
			for (std::thread &t: threadlist) {
			  if (t.joinable()) {
				t.join();
			  }
			}

			threadlist.clear();
		}

		if (i%50000 == 0) {
//			ofile<<"i: "<<i<<endl;
//			ofile<<"Number of Queries: "<<query<<endl;
//			ofile<<"Total Query duration: "<<durationQuery<<endl;
//			ofile<<"Time per Query: "<<durationQuery/query<<endl<<endl;
//
			//cout<<i<<endl<<"Time per Query: "<<durationQuery/query<<endl;
//
//			ofile<<"Number of Events: "<<data<<endl;
//			ofile<<"Total Event duration: "<<durationData<<endl;
//			ofile<<"Time per Event: "<<durationData/data<<endl<<endl;
//
			//cout<<"Time per Data: "<<durationData/data<<endl<<endl;
//
//			ofile<<"Number of operation: "<<data+query<<endl;
//			ofile<<"Time taken of all operation: "<<(durationData+durationQuery)<<endl;
//			ofile<<"Time per operation: "<<(durationData+durationQuery)/(data+query)<<endl<<endl<<endl;

//			for (std::thread &t: threadlist) {
//			      if (t.joinable()) {
//			        t.join();
//			      }
//			    }


			ofile<< i <<"," << (durationData+durationQuery)/i <<"," << query <<"," << durationQuery/query <<"," << data <<"," << durationData/data <<"," <<eventsc_rep/query<< endl;

		}
    }

    delete db;
    delete options.block_cache;
    delete options.Q_block_cache;

}
void cleandataset(char** argv)
{
	ifstream ifile(argv[1]);
	if (!ifile) { cerr << "Can't open input file " << endl; return; }

	ofstream ofile(argv[2],  std::ofstream::out | std::ofstream::app );
	if (!ofile) { cerr << "Can't open output file " << endl; return; }
	std::string line;
	long i = 0;
	int j=1;
	while(getline(ifile, line)) {

		if(i==0)
		{
			ofile<<"No," <<line<<endl;
			i++;
			continue;
		}

		if(i%10==0)
			ofile<< j++ <<"," << line <<endl;
		i++;
	}
}

int main(int argc, char** argv) {

	//test_frombenchmark();
	cleandataset(argv);
	//test_from_benchmark_repetative();

	return 0;
}

