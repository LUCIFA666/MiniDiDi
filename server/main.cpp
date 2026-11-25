#include "world.hpp"
#include "httplib.h"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <thread>

using namespace mini;
using Clock=std::chrono::steady_clock;
int main(int argc,char** argv){
    try{
        int port=8080,seed=42;std::string bind="127.0.0.1",web="web";
        for(int i=1;i<argc;++i){std::string a=argv[i];auto value=[&](){if(i+1>=argc)throw std::invalid_argument("missing argument for "+a);return std::string(argv[++i]);};
            if(a=="--port")port=std::stoi(value());else if(a=="--host")bind=value();else if(a=="--web")web=value();else if(a=="--seed")seed=std::stoi(value());
            else if(a=="--compare"){int s=42,steps=1200;if(i+1<argc)s=std::stoi(argv[++i]);if(i+1<argc)steps=std::stoi(argv[++i]);if(steps<1||steps>10000)throw std::invalid_argument("steps 1..10000");std::cout<<compare(s,steps).dump(2)<<'\n';return 0;}
            else throw std::invalid_argument("unknown argument: "+a);
        }
        if(port<1||port>65535)throw std::invalid_argument("port 1..65535");
        World world(seed);std::mutex mu;bool paused=false;double speed=3.0;std::atomic<bool> running{true};
        json phone={{"connected",false}};std::string matlab_session,phone_session;
        int matlab_index=-1;int64_t matlab_seq=-1,phone_seq=-1;
        Clock::time_point matlab_seen{},phone_seen{};
        std::mt19937_64 tokens(std::random_device{}());
        auto new_session=[&](){return std::to_string(tokens())+std::to_string(tokens());};
        httplib::Server app;app.set_payload_max_length(64*1024);
        app.set_read_timeout(5,0);app.set_write_timeout(5,0);
        app.set_exception_handler([](const auto&,auto& res,std::exception_ptr ep){std::string msg="request failed";try{if(ep)std::rethrow_exception(ep);}catch(const std::exception& e){msg=e.what();}res.status=400;res.set_content(json{{"error",msg}}.dump(),"application/json; charset=utf-8");});
        auto reply=[](auto& res,const json& body){res.set_content(body.dump(),"application/json; charset=utf-8");res.set_header("Cache-Control","no-store");};
        auto body=[](const httplib::Request& req)->json{auto j=json::parse(req.body);if(!j.is_object())throw std::invalid_argument("JSON object required");return j;};
        app.Get("/api/health",[&](const auto&,auto& res){reply(res,{{"ok",true},{"service","MiniDiDi"},{"version","1.0.0"}});});
        app.Get("/api/state",[&](const auto&,auto& res){std::lock_guard<std::mutex> lock(mu);auto s=world.state();s["paused"]=paused;s["speed"]=speed;s["phone"]=phone;s["tick_seconds"]=1;reply(res,s);});
        app.Get("/api/map",[&](const auto&,auto& res){reply(res,Grid().as_json());});
        app.Post("/api/control",[&](const auto& req,auto& res){auto j=body(req);std::lock_guard<std::mutex> lock(mu);
            double next_speed=j.value("speed",speed);bool next_paused=j.value("paused",paused);std::string next_strategy=j.value("strategy",world.strategy);
            if(!std::isfinite(next_speed)||next_speed<.5||next_speed>15)throw std::invalid_argument("speed 0.5..15");
            if(next_strategy!="nearest"&&next_strategy!="balanced")throw std::invalid_argument("strategy nearest/balanced");
            speed=next_speed;paused=next_paused;world.strategy=next_strategy;
            reply(res,{{"ok",true}});
        });
        app.Post("/api/reset",[&](const auto& req,auto& res){auto j=body(req);int s=j.value("seed",42);std::string policy=j.value("strategy",std::string("balanced"));World replacement(s,policy);std::lock_guard<std::mutex> lock(mu);
            world=std::move(replacement);matlab_index=-1;matlab_session.clear();phone_session.clear();matlab_seq=phone_seq=-1;phone={{"connected",false}};paused=false;reply(res,{{"ok",true},{"seed",s}});
        });
        app.Post("/api/orders",[&](const auto& req,auto& res){auto j=body(req);Cell a=j.at("pickup").template get<Cell>(),b=j.at("dropoff").template get<Cell>();std::lock_guard<std::mutex> lock(mu);world.add_order(a,b);reply(res,{{"ok",true},{"id",world.orders.back().id}});});
        app.Post("/api/compare",[&](const auto& req,auto& res){auto j=body(req);int steps=j.value("steps",1200);if(steps<100||steps>3000)throw std::invalid_argument("steps 100..3000");reply(res,compare(j.value("seed",42),steps));});
        app.Post("/api/devices/register",[&](const auto& req,auto& res){auto j=body(req);std::string kind=j.at("kind");std::lock_guard<std::mutex> lock(mu);
            if(kind=="matlab"){
                matlab_index=world.ensure_matlab();matlab_session=new_session();matlab_seq=-1;matlab_seen=Clock::now();world.cars[matlab_index].connected=true;
                reply(res,{{"session",matlab_session},{"id","M01"},{"map",world.grid.as_json()},{"vehicle",world.state()["vehicles"][matlab_index]}});
            }else if(kind=="phone"){
                phone_session=new_session();phone_seq=-1;phone_seen=Clock::now();phone={{"connected",true},{"id","P01"},{"pose",{{"east",0},{"north",0},{"yaw",0}}},{"samples",0}};
                world.log("device","手机数字孪生 P01 已接入独立实验区");reply(res,{{"session",phone_session},{"id","P01"},{"zone","phone-lab"},{"meters_per_unit",1.0}});
            }else throw std::invalid_argument("kind matlab/phone");
        });
        app.Post("/api/devices/matlab",[&](const auto& req,auto& res){auto j=body(req);std::lock_guard<std::mutex> lock(mu);
            if(matlab_index<0||j.at("session")!=matlab_session){res.status=401;reply(res,{{"error","register MATLAB first"}});return;}
            int64_t seq=j.at("seq").template get<int64_t>();if(seq<=matlab_seq)throw std::invalid_argument("out-of-order sequence");
            auto p=j.at("pose");world.matlab_pose(matlab_index,p.at("x").template get<double>(),p.at("y").template get<double>(),p.at("yaw").template get<double>(),j.at("command_id").template get<int>());
            matlab_seq=seq;matlab_seen=Clock::now();reply(res,{{"ok",true},{"tick",world.tick},{"vehicle",world.state()["vehicles"][matlab_index]},{"paused",paused}});
        });
        app.Post("/api/devices/phone",[&](const auto& req,auto& res){auto j=body(req);std::lock_guard<std::mutex> lock(mu);
            if(phone_session.empty()||j.at("session")!=phone_session){res.status=401;reply(res,{{"error","register phone first"}});return;}
            int64_t seq=j.at("seq").template get<int64_t>();if(seq<=phone_seq)throw std::invalid_argument("out-of-order sequence");
            auto p=j.at("pose");for(std::string k:{"east","north","yaw"}){double v=p.at(k).template get<double>();if(!std::isfinite(v)||std::abs(v)>10000)throw std::invalid_argument("invalid phone pose");}
            // Fixed schema prevents arbitrary telemetry from being reflected as UI markup.
            phone={{"connected",true},{"id","P01"},{"pose",p},{"samples",seq},{"sensors",j.value("sensors",json::object())},{"gps",j.value("gps",json::object())},{"last_tick",world.tick}};
            phone_seq=seq;phone_seen=Clock::now();reply(res,{{"ok",true},{"tick",world.tick}});
        });
        app.Get("/api/export",[&](const auto&,auto& res){std::lock_guard<std::mutex> lock(mu);json all=json::array();for(auto& o:world.orders)all.push_back(o.as_json());auto s=world.state();s["orders"]=all;reply(res,s);res.set_header("Content-Disposition","attachment; filename=MiniDiDi-run.json");});
        if(!app.set_mount_point("/",web))throw std::runtime_error("web directory missing: "+web);
        if(!app.bind_to_port(bind,port))throw std::runtime_error("cannot bind port "+std::to_string(port));
        std::thread simulation([&](){
            auto last=Clock::now();double accumulator=0;
            while(running){std::this_thread::sleep_for(std::chrono::milliseconds(20));auto now=Clock::now();double dt=std::chrono::duration<double>(now-last).count();last=now;
                try{std::lock_guard<std::mutex> lock(mu);
                    if(matlab_index>=0&&std::chrono::duration<double>(now-matlab_seen).count()>5)world.cars[matlab_index].connected=false;
                    if(!phone_session.empty()&&std::chrono::duration<double>(now-phone_seen).count()>5)phone["connected"]=false;
                    if(paused){accumulator=0;continue;}accumulator+=std::min(dt,.5)*speed;
                    if(accumulator>=1){accumulator-=1;world.step();}
                }catch(const std::exception& e){std::lock_guard<std::mutex> lock(mu);paused=true;world.log("error",e.what());std::cerr<<"Simulation paused: "<<e.what()<<'\n';}
            }
        });
        std::cout<<"MiniDiDi ready: http://"<<bind<<":"<<port<<"\nAutonomous fleet running. Ctrl+C to stop.\n"<<std::flush;
        app.listen_after_bind();running=false;simulation.join();return 0;
    }catch(const std::exception& e){std::cerr<<"MiniDiDi: "<<e.what()<<'\n';return 1;}
}
