#include "world.hpp"
#include <iostream>
#include <functional>
using namespace mini;
void check(bool condition,const std::string& message){if(!condition)throw std::runtime_error(message);}
void rejects(const std::function<void()>& f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}check(rejected,"invalid input was accepted");}
int main(){try{
    Grid g;auto p=g.path({0,0},{22,18});check(!p.empty()&&p.front()==Cell{0,0}&&p.back()==Cell{22,18},"A* endpoints");
    check(int(p.size())==41,"A* optimal length");for(size_t i=1;i<p.size();++i)check(g.road(p[i])&&manhattan(p[i-1],p[i])==1,"A* obstacle/adjacency");
    check(g.path({2,2},{0,0}).empty(),"blocked origin");
    check(hungarian({{9,2,7},{6,4,3},{5,8,1}})==std::vector<int>({1,0,2}),"Hungarian global optimum");
    World a(17,"nearest",false),b(17,"balanced",false);check(a.workload.size()==b.workload.size(),"workload length");
    for(size_t i=0;i<a.workload.size();++i)check(a.workload[i].pickup==b.workload[i].pickup&&a.workload[i].dropoff==b.workload[i].dropoff,"identical demand");
    for(int seed:{1,7,42})for(std::string policy:{"nearest","balanced"}){
        World w(seed,policy,false);for(int t=0;t<1600;++t){std::vector<Cell> old;for(auto& c:w.cars)old.push_back(c.cell);w.step();std::set<Cell> occupied;
            for(size_t i=0;i<w.cars.size();++i){check(occupied.insert(w.cars[i].cell).second,"vertex collision");check(g.road(w.cars[i].cell),"static collision");for(size_t j=i+1;j<w.cars.size();++j)check(!(old[i]==w.cars[j].cell&&old[j]==w.cars[i].cell),"edge swap");}
        }
        check(w.metrics()["completed"]==80,"finite workload did not finish seed="+std::to_string(seed)+" strategy="+policy+" count="+w.metrics()["completed"].dump());
        std::cout<<"PASS seed="<<seed<<" strategy="<<policy<<" completed=80 collisions=0\n";
    }
    World w;rejects([&]{w.add_order({2,2},{0,0});});rejects([&]{World bad(1,"bad");});
    int m=w.ensure_matlab();check(w.ensure_matlab()==m,"MATLAB re-registration duplicated car");
    rejects([&]{w.matlab_pose(m,999,999,0,0);});
    // Real HTTP transport is tested separately; this test drives the device authority contract.
    for(int t=0;t<400;++t){w.step();auto c=w.cars[m];if(c.has_command)w.matlab_pose(m,c.command.x,c.command.y,0,c.command_id);}
    check(w.cars[m].completed>0,"MATLAB vehicle never completed an order");
    rejects([&]{w.matlab_pose(m,w.cars[m].px,w.cars[m].py,0,-1);});
    std::cout<<"PASS external authority, reconnect, corridor validation\nAll core checks passed.\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
