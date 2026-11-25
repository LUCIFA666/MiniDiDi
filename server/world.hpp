#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <queue>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>
#include "json.hpp"

namespace mini {
using json = nlohmann::json;
struct Cell {
    int x=0, y=0;
    bool operator==(Cell b) const { return x==b.x && y==b.y; }
    bool operator!=(Cell b) const { return !(*this==b); }
    bool operator<(Cell b) const { return std::tie(x,y)<std::tie(b.x,b.y); }
};
inline void to_json(json& j,const Cell& p){j=json{{"x",p.x},{"y",p.y}};}
inline void from_json(const json& j,Cell& p){if(!j.at("x").is_number_integer()||!j.at("y").is_number_integer())throw std::invalid_argument("cell coordinates must be integers");p.x=j.at("x").get<int>();p.y=j.at("y").get<int>();}
inline int manhattan(Cell a,Cell b){return std::abs(a.x-b.x)+std::abs(a.y-b.y);}

struct Grid {
    int width=23,height=19;
    bool road(Cell p) const {
        if(p.x<0||p.y<0||p.x>=width||p.y>=height)return false;
        return !(p.x%6>=2 && p.x%6<=3 && p.y%6>=2 && p.y%6<=3);
    }
    std::vector<Cell> cells() const {
        std::vector<Cell> r;
        for(int y=0;y<height;++y)for(int x=0;x<width;++x)if(road({x,y}))r.push_back({x,y});
        return r;
    }
    std::vector<Cell> neighbors(Cell p,bool wait=false) const {
        std::vector<Cell> r;
        for(auto d:std::array<Cell,4>{{{1,0},{0,1},{-1,0},{0,-1}}}){
            Cell q{p.x+d.x,p.y+d.y};if(road(q))r.push_back(q);
        }
        if(wait)r.push_back(p);
        return r;
    }
    std::vector<Cell> path(Cell start,Cell goal,const std::set<Cell>& blocked={}) const {
        if(!road(start)||!road(goal)||blocked.count(goal))return {};
        using Node=std::tuple<int,int,int,int>;
        std::priority_queue<Node,std::vector<Node>,std::greater<Node>> open;
        std::map<Cell,int> distance;std::map<Cell,Cell> parent;
        open.emplace(manhattan(start,goal),0,start.x,start.y);distance[start]=0;
        while(!open.empty()){
            auto [f,g,x,y]=open.top();open.pop();(void)f;
            Cell p{x,y};if(distance[p]!=g)continue;
            if(p==goal){std::vector<Cell> r{p};while(p!=start){p=parent[p];r.push_back(p);}std::reverse(r.begin(),r.end());return r;}
            for(Cell q:neighbors(p))if(!blocked.count(q)&&(!distance.count(q)||g+1<distance[q])){
                distance[q]=g+1;parent[q]=p;open.emplace(g+1+manhattan(q,goal),g+1,q.x,q.y);
            }
        }return {};
    }
    json as_json() const {
        std::vector<Cell> blocks;
        for(int y=0;y<height;++y)for(int x=0;x<width;++x)if(!road({x,y}))blocks.push_back({x,y});
        return {{"width",width},{"height",height},{"cell_meters",1.0},{"obstacles",blocks},{"version",1}};
    }
};

struct Order {
    int id=0,arrival=0,assigned=-1,picked=-1,finished=-1,car=-1;
    Cell pickup,dropoff;
    json as_json() const {
        return {{"id",id},{"arrival",arrival},{"pickup",pickup},{"dropoff",dropoff},{"car",car},
                {"assigned",assigned},{"picked",picked},{"finished",finished},
                {"status",finished>=0?"completed":picked>=0?"onboard":assigned>=0?"assigned":"waiting"}};
    }
};
struct Vehicle {
    std::string id,kind="virtual";
    Cell cell,goal,command;
    double px=0,py=0,yaw=0;
    int order=-1,distance=0,completed=0,blocked_ticks=0,command_id=0;
    bool connected=true,has_command=false;
    std::vector<Cell> route;
};
struct Request {int arrival;Cell pickup,dropoff;};

// Rectangular Hungarian algorithm: each row receives a distinct column.
inline std::vector<int> hungarian(const std::vector<std::vector<double>>& cost){
    int n=int(cost.size());if(!n)return {};int m=int(cost[0].size());
    if(n>m)throw std::invalid_argument("assignment columns must cover rows");
    std::vector<double> u(n+1),v(m+1);std::vector<int> p(m+1),way(m+1);
    for(int i=1;i<=n;++i){
        p[0]=i;int j0=0;std::vector<double> best(m+1,1e18);std::vector<bool> used(m+1);
        do{
            used[j0]=true;int i0=p[j0],j1=0;double delta=1e18;
            for(int j=1;j<=m;++j)if(!used[j]){double cur=cost[i0-1][j-1]-u[i0]-v[j];if(cur<best[j]){best[j]=cur;way[j]=j0;}if(best[j]<delta){delta=best[j];j1=j;}}
            for(int j=0;j<=m;++j){if(used[j]){u[p[j]]+=delta;v[j]-=delta;}else best[j]-=delta;}
            j0=j1;
        }while(p[j0]!=0);
        do{int j1=way[j0];p[j0]=p[j1];j0=j1;}while(j0);
    }
    std::vector<int> assignment(n,-1);for(int j=1;j<=m;++j)if(p[j])assignment[p[j]-1]=j-1;
    return assignment;
}

class World {
public:
    Grid grid;
    std::vector<Vehicle> cars;
    std::vector<Order> orders;
    std::vector<Request> workload;
    std::vector<json> events;
    int tick=0,seed=42,next_request=0,collision_count=0,avoided_moves=0;
    std::string strategy="balanced";
    bool generate_forever=true;
    std::mt19937 rng;
    explicit World(int seed_=42,std::string strategy_="balanced",bool endless=true)
        :seed(seed_),strategy(std::move(strategy_)),generate_forever(endless),rng(uint32_t(seed_)){
        if(strategy!="nearest"&&strategy!="balanced")throw std::invalid_argument("unknown strategy");
        std::array<Cell,5> starts{{{0,0},{22,0},{0,18},{22,18},{11,9}}};
        for(int i=0;i<5;++i){Vehicle c;c.id="V0"+std::to_string(i+1);c.cell=c.goal=c.command=starts[i];c.px=c.cell.x;c.py=c.cell.y;cars.push_back(c);}
        auto roads=grid.cells();std::uniform_int_distribution<int> choice(0,int(roads.size())-1);
        // Materialize exogenous demand before either strategy runs; dispatch never consumes this RNG.
        for(int i=0;i<80;++i){Cell a=roads[choice(rng)],b=roads[choice(rng)];while(a==b)b=roads[choice(rng)];workload.push_back({i*7,a,b});}
        log("system","五车自主调度已启动");
    }
    void log(const std::string& type,const std::string& message){
        events.push_back({{"tick",tick},{"type",type},{"message",message}});
        if(events.size()>80)events.erase(events.begin());
    }
    void add_order(Cell a,Cell b){
        if(!grid.road(a)||!grid.road(b)||a==b)throw std::invalid_argument("pickup/dropoff must be different road cells");
        if(orders.size()>=5000)throw std::invalid_argument("order limit reached; reset scenario");
        Order o;o.id=int(orders.size())+1;o.arrival=tick;o.pickup=a;o.dropoff=b;orders.push_back(o);
        log("order","订单 #"+std::to_string(o.id)+" 等待派车");
    }
    void demand(){
        while(next_request<int(workload.size())&&workload[next_request].arrival<=tick){auto q=workload[next_request++];add_order(q.pickup,q.dropoff);}
        if(generate_forever&&next_request==int(workload.size())&&tick>workload.back().arrival&&tick%7==0){
            // Reuse the deterministic demand cycle, avoiding unlimited memory growth in long demos.
            int cycle=tick/7;const auto& q=workload[size_t(cycle)%workload.size()];
            if(orders.size()<5000)add_order(q.pickup,q.dropoff);
        }
    }
    void dispatch(){
        std::vector<int> idle,pending;
        for(int i=0;i<int(cars.size());++i)if(cars[i].connected&&cars[i].order<0)idle.push_back(i);
        for(int i=0;i<int(orders.size())&&pending.size()<20;++i)if(orders[i].assigned<0)pending.push_back(i);
        if(idle.empty()||pending.empty())return;
        auto assign=[&](int ci,int oi){auto& c=cars[ci];auto& o=orders[oi];c.order=oi;c.goal=o.pickup;o.car=ci;o.assigned=tick;log("dispatch",c.id+" 接单 #"+std::to_string(o.id));};
        if(strategy=="nearest"){
            for(int oi:pending){if(idle.empty())break;auto it=std::min_element(idle.begin(),idle.end(),[&](int a,int b){return grid.path(cars[a].cell,orders[oi].pickup).size()<grid.path(cars[b].cell,orders[oi].pickup).size();});assign(*it,oi);idle.erase(it);}
        }else{
            int m=std::max(idle.size(),pending.size());std::vector<std::vector<double>> cost(idle.size(),std::vector<double>(m,1e6));
            for(int i=0;i<int(idle.size());++i)for(int j=0;j<int(pending.size());++j){
                auto& c=cars[idle[i]];auto& o=orders[pending[j]];
                double pickup=double(grid.path(c.cell,o.pickup).size())-1;
                double trip=double(grid.path(o.pickup,o.dropoff).size())-1;
                cost[i][j]=pickup+0.35*trip+3.0*c.completed-0.15*(tick-o.arrival);
            }
            auto a=hungarian(cost);for(int i=0;i<int(a.size());++i)if(a[i]>=0&&a[i]<int(pending.size()))assign(idle[i],pending[a[i]]);
        }
    }
    void arrivals(){
        for(auto& c:cars)if(c.order>=0&&c.cell==c.goal&&!c.has_command){
            auto& o=orders[c.order];
            if(o.picked<0){o.picked=tick;c.goal=o.dropoff;log("pickup",c.id+" 已接到乘客 #"+std::to_string(o.id));}
            else{o.finished=tick;++c.completed;c.order=-1;c.goal=c.cell;c.route.clear();log("complete",c.id+" 完成订单 #"+std::to_string(o.id));}
        }
    }
    // A windowed space-time A* reserves vertices and reverse edges, including waiting.
    std::vector<Cell> cooperative_path(Cell start,Cell goal,const std::map<std::pair<int,Cell>,int>& vertices,
                                      const std::set<std::tuple<int,Cell,Cell>>& edges,int owner) const {
        constexpr int H=56;
        using Key=std::pair<int,Cell>;using Node=std::tuple<int,int,int,int>;
        std::priority_queue<Node,std::vector<Node>,std::greater<Node>> open;
        std::set<Key> seen;std::map<Key,Key> parent;
        Key initial{0,start},best=initial;int best_h=manhattan(start,goal);
        auto can_hold=[&](Cell p,int t){for(int future=t+1;future<=H;++future){auto it=vertices.find({future,p});if(it!=vertices.end()&&it->second!=owner)return false;}return true;};
        open.emplace(best_h,0,start.x,start.y);seen.insert(initial);
        while(!open.empty()){
            auto [f,t,x,y]=open.top();open.pop();(void)f;Cell p{x,y};Key k{t,p};
            int h=manhattan(p,goal);
            if(h<best_h&&can_hold(p,t)){best=k;best_h=h;}
            if(p==goal&&can_hold(p,t)){best=k;break;}
            if(t>=H)continue;
            for(Cell q:grid.neighbors(p,true)){
                auto it=vertices.find({t+1,q});
                if(it!=vertices.end()&&it->second!=owner)continue;
                if(edges.count({t+1,q,p}))continue;
                Key nk{t+1,q};if(!seen.insert(nk).second)continue;
                parent[nk]=k;open.emplace(t+1+manhattan(q,goal),t+1,q.x,q.y);
            }
        }
        std::vector<Cell> r{best.second};while(best!=initial){best=parent.at(best);r.push_back(best.second);}std::reverse(r.begin(),r.end());return r;
    }
    void move(){
        constexpr int H=56;
        std::map<std::pair<int,Cell>,int> vertices;std::set<std::tuple<int,Cell,Cell>> edges;
        for(int i=0;i<int(cars.size());++i){
            for(int t=1;t<=H;++t){vertices[{t,cars[i].cell}]=i;if(cars[i].has_command)vertices[{t,cars[i].command}]=i;}
        }
        std::vector<int> priority;for(int i=0;i<int(cars.size());++i)priority.push_back(i);
        std::stable_sort(priority.begin(),priority.end(),[&](int a,int b){
            int pa=cars[a].blocked_ticks*10+(a+tick)%int(cars.size());int pb=cars[b].blocked_ticks*10+(b+tick)%int(cars.size());return pa>pb;
        });
        std::vector<Cell> next;for(auto& c:cars)next.push_back(c.cell);
        for(int i:priority){auto& c=cars[i];
            if(c.kind!="virtual"||c.order<0)continue;
            for(int t=1;t<=H;++t){auto it=vertices.find({t,c.cell});if(it!=vertices.end()&&it->second==i)vertices.erase(it);}
            c.route=cooperative_path(c.cell,c.goal,vertices,edges,i);
            if(c.route.size()>1)next[i]=c.route[1];
            for(int t=1;t<=H;++t){Cell p=c.route[std::min(size_t(t),c.route.size()-1)];Cell prev=c.route[std::min(size_t(t-1),c.route.size()-1)];vertices[{t,p}]=i;edges.insert({t,prev,p});}
            if(next[i]==c.cell){++c.blocked_ticks;++avoided_moves;}else c.blocked_ticks=0;
        }
        // Safety checks are independent of the planner and stay active in release builds.
        for(int i=0;i<int(cars.size());++i){
            if(!grid.road(next[i]))throw std::logic_error("static obstacle collision");
            for(int j=i+1;j<int(cars.size());++j)
                if(next[i]==next[j]||(next[i]==cars[j].cell&&next[j]==cars[i].cell)){
                    ++collision_count;throw std::logic_error("reservation collision");
                }
        }
        for(int i=0;i<int(cars.size());++i)if(cars[i].kind=="virtual"){
            auto& c=cars[i];if(next[i]!=c.cell){c.yaw=std::atan2(next[i].y-c.cell.y,next[i].x-c.cell.x);++c.distance;}c.cell=next[i];c.px=c.cell.x;c.py=c.cell.y;
        }
        // MATLAB endpoints remain reserved until the controller acknowledges the command.
        for(int i=0;i<int(cars.size());++i){auto& c=cars[i];if(c.kind!="matlab"||!c.connected||c.has_command||c.order<0||c.cell==c.goal)continue;
            std::set<Cell> blocked;
            for(int j=0;j<int(cars.size());++j)if(i!=j){blocked.insert(cars[j].cell);if(cars[j].has_command)blocked.insert(cars[j].command);}
            c.route=grid.path(c.cell,c.goal,blocked);
            if(c.route.size()>1){c.command=c.route[1];c.has_command=true;++c.command_id;}
        }
    }
    void step(){demand();arrivals();dispatch();arrivals();move();++tick;arrivals();}
    int ensure_matlab(){
        for(int i=0;i<int(cars.size());++i)if(cars[i].kind=="matlab")return i;
        std::set<Cell> used;for(auto& c:cars){used.insert(c.cell);if(c.has_command)used.insert(c.command);}
        Cell p{10,0};for(auto q:grid.cells())if(!used.count(q)){p=q;break;}
        Vehicle c;c.id="M01";c.kind="matlab";c.cell=c.goal=c.command=p;c.px=p.x;c.py=p.y;cars.push_back(c);
        log("device","MATLAB 模拟车 M01 已接入");return int(cars.size())-1;
    }
    void matlab_pose(int ci,double x,double y,double yaw,int command_id){
        auto& c=cars.at(ci);
        if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(yaw))throw std::invalid_argument("non-finite pose");
        Cell end=c.has_command?c.command:c.cell;
        if(command_id!=c.command_id){
            // A new command may be issued while a stationary heartbeat is in flight.
            // Accept that heartbeat at the current cell and return the fresh command.
            if(c.has_command&&command_id==c.command_id-1&&std::hypot(x-c.cell.x,y-c.cell.y)<.04){c.connected=true;return;}
            throw std::invalid_argument("stale command id; reconnect or read command");
        }
        double dx=end.x-c.cell.x,dy=end.y-c.cell.y,len=dx*dx+dy*dy;
        double t=len?((x-c.cell.x)*dx+(y-c.cell.y)*dy)/len:0;
        double ex=x-(c.cell.x+std::clamp(t,0.0,1.0)*dx),ey=y-(c.cell.y+std::clamp(t,0.0,1.0)*dy);
        if(t<-.05||t>1.05||std::hypot(ex,ey)>.16)throw std::invalid_argument("pose outside reserved corridor");
        c.px=x;c.py=y;c.yaw=yaw;c.connected=true;
        if(c.has_command&&std::hypot(x-end.x,y-end.y)<.04){c.cell=end;c.px=end.x;c.py=end.y;c.has_command=false;++c.distance;arrivals();}
    }
    json metrics() const {
        int complete=0,picked=0,active=0,pending=0,total_distance=0;double wait=0,journey=0,outstanding=0;
        for(auto& o:orders){if(o.picked>=0){++picked;wait+=o.picked-o.arrival;}else outstanding+=tick-o.arrival;if(o.finished>=0){++complete;journey+=o.finished-o.arrival;}if(o.assigned<0)++pending;}
        for(auto& c:cars){total_distance+=c.distance;if(c.order>=0)++active;}
        return {{"completed",complete},{"orders",orders.size()},{"picked",picked},{"pending",pending},
            {"active_vehicles",active},{"mean_wait_ticks",picked?wait/picked:0.0},
            {"mean_journey_ticks",complete?journey/complete:0.0},{"outstanding_wait_ticks",outstanding},
            {"total_distance",total_distance},{"collisions",collision_count},{"yield_events",avoided_moves},
            {"completion_rate",orders.empty()?0.0:double(complete)/orders.size()}};
    }
    json state() const {
        json cs=json::array(),os=json::array();
        for(auto& c:cars)cs.push_back({{"id",c.id},{"kind",c.kind},{"cell",c.cell},{"position",{{"x",c.px},{"y",c.py}}},
            {"yaw",c.yaw},{"goal",c.goal},{"route",c.route},{"distance",c.distance},{"completed",c.completed},
            {"connected",c.connected},{"order",c.order<0?0:orders[c.order].id},{"status",c.order<0?"idle":orders[c.order].picked<0?"pickup":"occupied"},
            {"command_id",c.command_id},{"waypoint",c.has_command?c.command:c.cell},{"has_command",c.has_command}});
        for(size_t i=orders.size()>100?orders.size()-100:0;i<orders.size();++i)os.push_back(orders[i].as_json());
        return {{"tick",tick},{"seed",seed},{"strategy",strategy},{"map",grid.as_json()},{"vehicles",cs},{"orders",os},{"metrics",metrics()},{"events",events}};
    }
};
inline json compare(int seed,int steps){
    json result={{"seed",seed},{"steps",steps},{"order_count",80},{"vehicle_count",5},{"results",json::array()}};
    for(std::string policy:{"nearest","balanced"}){World w(seed,policy,false);for(int t=0;t<steps;++t)w.step();result["results"].push_back({{"strategy",policy},{"metrics",w.metrics()}});}
    result["note"]="同地图、同初始车辆、同到达时刻和起终点；不接入外部设备。等待时间仅统计已接到乘客的订单，同时报告未完成数和未上车累计等待，避免只比较已完成样本。";
    return result;
}
} // namespace mini
