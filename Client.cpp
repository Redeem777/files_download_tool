#include<iostream>
#include<ifaddrs.h>
#include<sys/types.h>
#include"httplib.h"
#include<boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include<boost/thread.hpp>
using std::cout;
using std::endl;
using std::vector;
using std::string;
using namespace httplib;
namespace bf = boost::filesystem;
#define RANGE_SIZE 300
class P2PClient{
public:
	uint16_t _srv_port;
	size_t _host_idx;
	vector<string> _online_list;
	vector<string> _file_list;
	bool GetALLHost(vector<string>&list)
	{
		struct ifaddrs *addrs = NULL;
		struct sockaddr_in *ip = NULL;
		struct sockaddr_in *mask = NULL;
		getifaddrs(&addrs);
		for (; addrs != NULL; addrs = addrs->ifa_next)
		{
			ip = (struct sockaddr_in *)addrs->ifa_addr;
			mask = (struct sockaddr_in *)addrs->ifa_netmask;
			if (ip->sin_family != AF_INET)
			{
				continue;
			}
			if (ip->sin_addr.s_addr == inet_addr("127.0.0.1") ||
				ip->sin_addr.s_addr == inet_addr("192.168.182.130")){
				continue;
			}
			uint32_t net, host;
			net = ntohl(ip->sin_addr.s_addr & mask->sin_addr.s_addr);
			host = ntohl(~mask->sin_addr.s_addr);
			for (uint32_t i = 2; i < host - 1; i++)
				//for(uint32_t i = 146;i < 149;i++)
			{
				struct in_addr ip;
				ip.s_addr = htonl(net + i);
				cout << inet_ntoa(ip) << endl;
				list.push_back(inet_ntoa(ip));
			}
		}
		freeifaddrs(addrs);
		return true;
	}
	/*bool GetOnlineHost(vector<string> &list)
	{
	_online_list.clear();
	for(auto &i:list)
	{
	Client client(i.c_str(),_srv_port);
	auto rsp = client.Get("/hostpair");
	if(rsp && rsp->status == 200 )
	{
	std::cerr<<"host"<<i<<"pair success";
	cout<<endl;
	_online_list.push_back(i);
	continue;
	}
	std::cerr<<"host"<<i<<"pair failed"<<endl;
	}
	return true;
	}*/
	void HostPair(string &i)
	{
		Client client(i.c_str(), _srv_port);
		auto rsp = client.Get("/hostpair");
		if (rsp && rsp->status == 200)
		{
			std::cerr << "host" << i << "pair success" << endl;
			_online_list.push_back(i);
		}
		std::cerr << "host" << i << "pair failed" << endl;
	}
	bool GetOnlineHost(vector<string> &list)
	{
		_online_list.clear();
		vector<std::thread>thr_list(list.size());
		for (size_t i = 0; i < thr_list.size(); i++)
		{
			std::thread thr(&P2PClient::HostPair, this, std::ref(list[i]));
			thr_list[i] = std::move(thr);
			//std::thread thr(&P2PClient::HostPair,this,i);
			//std::cerr<<"host "<<i <<"pair failed\n";
		}
		for (auto&i : thr_list)
		{
			i.join();
		}
		return true;
	}
	bool ShowOlineHost()
	{
		for (size_t i = 0; i < _online_list.size(); i++)
		{
			cout << i << "." << _online_list[i] << endl;
		}
		cout << "please choose : ";
		fflush(stdout);
		std::cin >> _host_idx;
		if (_online_list.size() == 0 && _host_idx == 0)
		{
			cout << "choose error" << endl;
			return false;
		}
		if (_host_idx < 0 || _host_idx > _online_list.size())
		{
			_host_idx = -1;
			cout << "choose error" << endl;
			return false;
		}
		return true;
	}
	bool GetFilelist()
	{
		Client client(_online_list[_host_idx].c_str(), _srv_port);
		auto rsp = client.Get("/list");
		if (rsp && rsp->status == 200)
			boost::split(_file_list, rsp->body, boost::is_any_of("\n"));
		return true;
	}
	bool ShowFileList(string &name)
	{
		for (size_t i = 0; i < _file_list.size(); i++)
		{
			cout << i << " ." << _file_list[i] << endl;
		}
		cout << "please choose : " << endl;
		size_t file_idx = 0;
		std::cin >> file_idx;
		if (file_idx < 0 || file_idx > _file_list.size())
		{
			cout << "file_idx choose error\n";
			return false;
		}
		name = _file_list[file_idx];
		return true;
	}
	/*bool DownLoadFile(string &name)
	{
	string host = _online_list[_host_idx];
	string uri = "/list/" + name;
	Client client(host.c_str(),_srv_port);
	auto rsp = client.Get(uri.c_str());
	string realpath = "DOWN_LOAD/" + name;
	if(rsp && rsp->status == 200)
	{
	std::ofstream file(realpath.c_str(),std::ios::binary);
	if(!file.is_open())
	{
	std::cerr<<"open file error"<<endl;
	return false;
	}
	file.write(&rsp->body[0],rsp->body.size());
	if(!file.good())
	{
	std::cerr<<"file "<<realpath<<" write body error"<<endl;
	return false;
	}
	file.close();
	std::cerr<<"file "<<realpath<<" DownLoadFile success!!"<<endl;
	}else{
	std::cerr<<"file "<<realpath<<" DownLoadFile failed!!"<<endl;
	return false;
	}
	return true;
	}*/
	void RangeDownLoad(std::string &name, int64_t &start, int64_t &len, int&res, string &host, int64_t &end)
	{
		res = 0;
		string realpath = "DOWN_LOAD/" + name;
		string uri = "/list/" + name;
		std::stringstream range_val;
		range_val << "bytes=" << start << "-" << end;
		Client client(host.c_str(), _srv_port);
		Headers header;
		header.insert(std::make_pair("Range", range_val.str().c_str()));
		auto rsp = client.Get(uri.c_str(), header);
		if (rsp && rsp->status == 206)
		{
			std::ofstream file(realpath.c_str(), std::ios::binary);
			if (!file.is_open())
			{
				std::cerr << "open" << realpath << "error" << endl;
				return;
			}
			file.seekp(start, std::ios::beg);
			file.write(rsp->body.c_str(), len);
			if (!file.good())
			{
				std::cerr << "file " << realpath << " error" << endl;
				file.close();
				file.close();
				return;
			}
			file.close();
			res = 1;
			cout << "file" << realpath << " download range:";
			cout << range_val.str() << " success " << endl;
		}
	}
	int64_t GetFileSize(string &host, string &name)
	{
		int64_t fsize = -1;
		string path = "/list/" + name;
		Client client(host.c_str(), _srv_port);
		auto rsp = client.Head(path.c_str());
		if (rsp && rsp->status == 200)
		{
			if (!rsp->has_header("Content-Length"))
			{
				return -1;
			}
			string len = rsp->get_header_value("Content-Length");
			std::stringstream tmp;
			tmp << len;
			tmp >> fsize;
		}
		return fsize;
	}
	bool DownLoadFile(string &name)
	{
		string host = _online_list[_host_idx];
		int64_t fsize = GetFileSize(host, name);
		if (fsize < 0)
		{
			std::cerr << "download file" << name << "failed" << endl;
			return false;
		}
		string uri = "/list/" + name;
		int count = fsize / RANGE_SIZE;
		vector<std::thread> thr_list(count + 1);
		vector<int> res_list(count + 1);
		for (int i = 0; i <= count; i++)
		{
			int64_t start = i*RANGE_SIZE;
			int64_t end = (i + 1)*RANGE_SIZE - 1;
			if (i == count){
				if (fsize % RANGE_SIZE == 0)
					break;
				end = RANGE_SIZE - 1;
			}
			int64_t len = end - start + 1;
			//const bool &ret = res_list[i];
			std::thread thr(&P2PClient::RangeDownLoad, this, std::ref(name), std::ref(start), std::ref(len), std::ref(res_list[i]), std::ref(host), std::ref(end));
			//std::thread thr(&P2PClient::RangeDownLoad,this,name,start,len,res_list[i],host,end);
			thr_list[i] = std::move(thr);
		}
		bool ret = true;
		for (int i = 0; i <= count; i++)
		{
			thr_list[i].join();
			if (res_list[i] == true)
				continue;
			ret = false;
		}
		if (ret == false)
		{
			std::cerr << "download file" << name << " failed" << endl;
			return false;
		}
		if (ret == true)
			std::cerr << "download file" << name << " sucess" << endl;
		return true;
	}
	int DoFace()
	{
		cout << "1.搜素附近主机" << endl;
		cout << "2.显示在线主机" << endl;
		cout << "3.显示文件列表" << endl;
		cout << "0.退出" << endl;
		int choose;
		std::cin >> choose;
		return choose;
	}
public:
	bool Start(uint16_t port)
	{
		_srv_port = port;
		vector<string>list;
		string filename;
		while (1)
		{
			int choose = DoFace();
			switch (choose)
			{
			case 1:
				GetALLHost(list);
				GetOnlineHost(list);
				break;
			case 2:
				if (ShowOlineHost() == false)
					break;
				GetFilelist();
			case 3:
				if (ShowFileList(filename) == false)
					break;
				DownLoadFile(filename);
				break;
			case 4:
				break;
			}
		}
		return true;
	}
};
int main()
{
	P2PClient cli;
	cli.Start(9000);
}
