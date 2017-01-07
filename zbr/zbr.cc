// File:    zbr.cc
// Mode:    C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t

// Copyright (c) 2003-2004 Samsung Advanced Institute of Technology and
// The City University of New York. All rights reserved.

// ZigBee Routing (ZBR) is based on ZBR and Cluster-Tree.
// The ZBR part presented here is from Carnegie Mellon University's ns2
// ZBR simulation code (see the following copyright notice)


/*
Copyright (c) 1997, 1998 Carnegie Mellon University.  All Rights
Reserved.

Permission to use, copy, modify, and distribute this
software and its documentation is hereby granted (including for
commercial or for-profit use), provided that both the copyright notice and 
this permission notice appear in all copies of the software, derivative 
works, or modified versions, and any portions thereof, and that both notices 
appear in supporting documentation, and that credit is given to Carnegie 
Mellon University in all publications reporting on direct or indirect use of 
this code or its derivatives.

ALL CODE, SOFTWARE, PROTOCOLS, AND ARCHITECTURES DEVELOPED BY THE CMU
MONARCH PROJECT ARE EXPERIMENTAL AND ARE KNOWN TO HAVE BUGS, SOME OF
WHICH MAY HAVE SERIOUS CONSEQUENCES. CARNEGIE MELLON PROVIDES THIS
SOFTWARE OR OTHER INTELLECTUAL PROPERTY IN ITS ``AS IS'' CONDITION,
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE OR
INTELLECTUAL PROPERTY, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
DAMAGE.

Carnegie Mellon encourages (but does not require) users of this
software or intellectual property to return any improvements or
extensions that they make, and to grant Carnegie Mellon the rights to 
redistribute these changes without encumbrance.

The ZBR code developed by the CMU/MONARCH group was optimized and tuned by 
Samir Das and Mahesh Marina, University of Cincinnati. The work was 
partially done in Sun Microsystems. Modified for gratuitous replies by Anant 
Utgikar, 09/16/02.

*/

#include <zbr/zbr.h>
#include <zbr/zbr_packet.h>
#include <random.h>
#include <cmu-trace.h>
#include <zbr/zbr_link.h>
#include <energy-model.h>

#define max(a,b)        ( (a) > (b) ? (a) : (b) )
#define CURRENT_TIME    Scheduler::instance().clock()

//#define ZBRDEBUG
//#define ERROR

#ifdef ZBRDEBUG
int extra_route_reply = 0;
int limit_route_request = 0;
int route_request = 0;
#endif

/*
  TCL Hooks
*/



//// zhu: Global constants are defined here.
/* 
=========================================================================== 
*/
/* Structure to store the networking layer's system parameters.              
   */
/* 
=========================================================================== 
*/
NET_SYSTEM_CONFIG NetSystemConfig = {
    85,			/* BlockSize */		//
    4,			/* Cm */		//
    3,			/* Lm */		//
    1,			/* Default Freq */
    300,		/* Ticks per frame */
    30,			/* Frames per update */
    6,			/* Name to address mapping update period */
    15,			/* Lower RSSI limit */
    20,			/* Middle RSSI limit */
    25			/* Upper RSSI limit */
};

//// end zhu

/*
  TCL Hooks
*/


int hdr_zbr::offset_;
static class ZBRHeaderClass : public PacketHeaderClass {
public:
        ZBRHeaderClass() : PacketHeaderClass("PacketHeader/ZBR",
                                              sizeof(hdr_all_zbr)) {
	  bind_offset(&hdr_zbr::offset_);
	}
} class_rtProtoZBR_hdr;

static class ZBRclass : public TclClass {
public:
        ZBRclass() : TclClass("Agent/ZBR") {}
        TclObject* create(int argc, const char*const* argv) {
          assert(argc == 5);
          //return (new ZBR((nsaddr_t) atoi(argv[4])));
          //printf("now my id is %s\n",argv[4]);
	  return (new ZBR((nsaddr_t) Address::instance().str2addr(argv[4])));
        }
	//
        virtual void bind();
        virtual int method(int argc, const char*const* argv);
        //
} class_rtProtoZBR;

//
void ZBRclass::bind()
{
        TclClass::bind();
        add_method("Cm");
        add_method("Lm");
        add_method("BSize");
        add_method("CSkip");
}
//这个是对Agent/ZBR的设置
int ZBRclass::method(int ac, const char*const* av)
{
        //bind global variables or static variables of class
        //here we bind
	//	NetSystemConfig.Cm
	//	NetSystemConfig.Lm
	//	NetSystemConfig.BlockSize

        int i;
        unsigned int j;
        const char *optlist[] = {"Cm","Lm","BSize","CSkip"};
        Tcl& tcl = Tcl::instance();
        int argc = ac - 2;
        const char*const* argv = av + 2;

	if (argc >=2)
	for (i=0;i<4;i++)
	{
		if (strcmp(argv[1], optlist[i]) == 0) break;
	}

        if (argc == 2)
        {
                j=99;
                switch(i)
                {
                case 0:
                        j=NetSystemConfig.Cm;
                        break;
                case 1:
                        j=NetSystemConfig.Lm;
                        break;
                case 2:
                        j=NetSystemConfig.BlockSize;
                        break;
                default:
                	break;
            	}
		tcl.resultf("%u",j);
		return (TCL_OK);
        }
        else if(argc == 3)
        {
		if ((i == 2)&&(strcmp(argv[2],"calc") == 0))
			NetSystemConfig.BlockSize = ZBR::Cskip_BSize();
		else
		{
			j = atoi(argv[2]);
	                switch(i)
	                {
	                case 0:
	                        NetSystemConfig.Cm = j;
	                        break;
	                case 1:
	                        NetSystemConfig.Lm = j;
	                        break;
	                case 2:
	                        NetSystemConfig.BlockSize = j;
	                        break;
	                case 3:
				j=ZBR::c_skip(j);
				tcl.resultf("%u",j);
                	        break;
	                default:
	                	break;
	            	}
	        }
		return (TCL_OK);
        }
        return TclClass::method(ac, av);
}
//上面的是关于Agent/ZBR 的设置
//这个是关于 节点的设置
int
ZBR::command(int argc, const char*const* argv) {
  Tcl& tcl = Tcl::instance();

  //
  if (strcmp(argv[1], "RNType") == 0)
  {
	if (argc == 2)		//get value
		tcl.resultf("%d",RNType);
	else if (argc == 3)	//set value
		RNType = atoi(argv[2]);
	return TCL_OK;
  }
  //

  if(argc == 2) {
    if(strncasecmp(argv[1], "id", 2) == 0) {
      tcl.resultf("%d", index);
      return TCL_OK;
    }

    if(strncasecmp(argv[1], "start", 2) == 0) {
      btimer.handle((Event*) 0);

#ifndef ZBR_LINK_LAYER_DETECTION
      ntimer.handle((Event*) 0);
#endif // LINK LAYER DETECTION

      rtimer.handle((Event*) 0);
      return TCL_OK;
     }
  }
  else if(argc == 3) {
    if(strcmp(argv[1], "index") == 0) {
      index = atoi(argv[2]);
      return TCL_OK;
    }

    else if(strcmp(argv[1], "log-target") == 0 || strcmp(argv[1], "tracetarget") == 0) {
      logtarget = (Trace*) TclObject::lookup(argv[2]);
      if(logtarget == 0)
	return TCL_ERROR;
      return TCL_OK;
    }
    else if(strcmp(argv[1], "drop-target") == 0) {
    int stat = rqueue.command(argc,argv);
      if (stat != TCL_OK) return stat;
      return Agent::command(argc, argv);
    }
    else if(strcmp(argv[1], "if-queue") == 0) {
    ifqueue = (PriQueue*) TclObject::lookup(argv[2]);

      if(ifqueue == 0)
	return TCL_ERROR;
      return TCL_OK;
    }
    else if (strcmp(argv[1], "port-dmux") == 0) {
    	dmux_ = (PortClassifier *)TclObject::lookup(argv[2]);
	if (dmux_ == 0) {
		fprintf (stderr, "%s: %s lookup of %s failed\n", __FILE__,
		argv[1], argv[2]);
		return TCL_ERROR;
	}
	return TCL_OK;
    }
  }

  /*
  else if(argc == 4) {
    if(strcmp(argv[1], "Add_Nb") == 0) {
		
		nb_insert(atoi(argv[2]), argv[3][0]);
		return TCL_OK;
    }
  }
  */

  return Agent::command(argc, argv);
}

/*
   Constructor
*/

ZBR::ZBR(nsaddr_t id) : Agent(PT_ZBR),
			  btimer(this), ntimer(this),
			  rtimer(this), lrtimer(this), rqueue() {

			  	//index 应该是唯一标志的地址吧
  index = id;
  RNType = 1;			//zheng: add -- default RN+
  chkAddZBRLink(index,this);	//zheng: add
  bid = 1;

  LIST_INIT(&nbhead);
  LIST_INIT(&bihead);

  logtarget = 0;
  ifqueue = 0;

  dRate = 20000.0;		//assume the lowest rate at this moment
	
  /*
  //// zhu: binding C++ variable to tcl

  //// Node information
  bind("NodeID",&myNodeID);
  bind("Depth",&myDepth);
  bind("ParentNodeID",&myParentNodeID);
  bind("RNType",&RNType);
  */
}

/*
  Timers
*/

void
ZBR_BroadcastTimer::handle(Event*) {
  agent->id_purge();
  Scheduler::instance().schedule(this, &intr, ZBR_BCAST_ID_SAVE);
}

void
ZBR_NeighborTimer::handle(Event*) {
  agent->nb_purge();
  Scheduler::instance().schedule(this, &intr, ZBR_HELLO_INTERVAL);
}

void
ZBR_RouteCacheTimer::handle(Event*) {
  agent->rt_purge();
#define ZBR_FREQUENCY 0.5 	// sec
  Scheduler::instance().schedule(this, &intr, ZBR_FREQUENCY);
}
//这个应该是一个修复的把？ 关于具体的修复场景？
//这个需要详细的了解 询问下
void
ZBR_LocalRepairTimer::handle(Event* p)  {  // SRD: 5/4/99
zbr_rt_entry *rt;
struct hdr_ip *ih = HDR_IP( (Packet *)p);

   /* you get here after the timeout in a local repair attempt */
   /*	fprintf(stderr, "%s\n", __FUNCTION__); */


    rt = agent->rtable.rt_lookup(ih->daddr());

    if (rt && rt->rt_flags != RTF_UP) {
    	//
	if (!rt->all_brct_flag)
	{
		rt->all_brct_flag = 1;				//zheng: let's try once more with  set
		rt->rt_req_timeout = 0.0;
	   	rt->rt_req_cnt = 0;
		rt->rt_req_last_ttl = ZBR_NETWORK_DIAMETER / 2 + 1;
		//rt->rt_req_last_ttl = ZBR_NETWORK_DIAMETER;
		agent->local_rt_repair(rt, (Packet *)p, true);
	}
	else if (rt->rt_req_cnt < ZBR_RREQ_RETRIES)
	{
		rt->rt_req_timeout = 0.0;
		rt->rt_req_last_ttl = ZBR_NETWORK_DIAMETER / 2 + 1;
		//rt->rt_req_last_ttl = ZBR_NETWORK_DIAMETER;
		agent->local_rt_repair(rt, (Packet *)p, true);
	}
	else
    	//
	{
		agent->rt_down(rt);
#ifdef ZBRDEBUG
		fprintf(stderr,"Node %d: Dst - %d, failed local repair\n",index, rt->rt_dst);
#endif
		Packet::free((Packet *)p);
	}
    }
    else
	Packet::free((Packet *)p);
}


/*
   Broadcast ID Management  Functions
*/


void
ZBR::id_insert(nsaddr_t id, u_int32_t bid) {
	ZBR_BroadcastID *b = new ZBR_BroadcastID(id, bid);
	
	assert(b);
	b->expire = CURRENT_TIME + ZBR_BCAST_ID_SAVE;
	LIST_INSERT_HEAD(&bihead, b, link);
}

/* SRD */
bool
ZBR::id_lookup(nsaddr_t id, u_int32_t bid) {
	ZBR_BroadcastID *b = bihead.lh_first;
	
	// Search the list for a match of source and bid
	for( ; b; b = b->link.le_next) {
	   if ((b->src == id) && (b->id == bid))
	     return true;
	}
	return false;
}
//清除生存时间过期的数据
void
ZBR::id_purge() {
	ZBR_BroadcastID *b = bihead.lh_first;
	ZBR_BroadcastID *bn;
	double now = CURRENT_TIME;
	
	for(; b; b = bn) {
	   bn = b->link.le_next;
	   if(b->expire <= now) {
	     LIST_REMOVE(b,link);
	     delete b;
	   }
	}
}

/*
  Helper Functions
*/
//这个应该是计算时延的吧？
double
ZBR::PerHopTime(zbr_rt_entry *rt) {
	int num_non_zero = 0, i;
	double total_latency = 0.0;
	
	if (!rt)
	   return ((double) ZBR_NODE_TRAVERSAL_TIME );
	
	for (i=0; i < MAX_HISTORY; i++) {
	   if (rt->rt_disc_latency[i] > 0.0) {
	      num_non_zero++;
	      total_latency += rt->rt_disc_latency[i];
	   }
	}
	if (num_non_zero > 0)
	   return(total_latency / (double) num_non_zero);
	else
	   return((double) ZBR_NODE_TRAVERSAL_TIME);
}

/*
  Link Failure Management Functions
*/
//test_for_dy
//还未懂
static void
zbr_rt_failed_callback(Packet *p, void *arg) {
	((ZBR*) arg)->rt_ll_failed(p);
}

/*
* This routine is invoked when the link-layer reports a route failed.
*/
void
ZBR::rt_ll_failed(Packet *p) {
	struct hdr_cmn *ch = HDR_CMN(p);
	struct hdr_ip *ih = HDR_IP(p);
	zbr_rt_entry *rt;
	nsaddr_t broken_nbr = ch->next_hop_;
	
	//NEW: Yong modified on 10/10/03 for ZigBee routing
	ZBR_Neighbor* nbr;
	nbr = nb_lookup (broken_nbr);
	//NEW END
	
	//FIX: added by Yong on 11/13/03
	if (nbr && (ch->xmit_reason_ == 1)) {
		nbr->NodeFailure = 0;
		nbr->FailCounter = 0;
		Packet::free(p);
		return;
	}
	//FIX END	
		
	
		
	//NEW: Yong: ZBR_LINK_LAYER_DETECTION has to be enabled for local repair
	#ifndef ZBR_LINK_LAYER_DETECTION
	drop(p, DROP_RTR_MAC_CALLBACK);
	#else
	//NEW END
	
	
	/*
	  * Non-data packets and Broadcast Packets can be dropped.
	  */
	   if(! DATA_PACKET(ch->ptype()) ||
            (u_int32_t) ih->daddr() == IP_BROADCAST)  {
	    drop(p, DROP_RTR_MAC_CALLBACK);
	    return;
	  }
	  log_link_broke(p);
	  
	//NEW: Yong modified on 10/10/03 for ZigBee routing
	/*	if((rt = rtable.rt_lookup(ih->daddr())) == 0) {
	    drop(p, DROP_RTR_MAC_CALLBACK);
	    return;
	  } */
	//NEW END  
	  
	  
	  log_link_del(ch->next_hop_);
	
	//NEW: Yong modified on 10/10/03 for ZigBee routing
	#ifdef ZBR_LOCAL_REPAIR
	
	if (!nbr)
	{
		drop(p, DROP_RTR_MAC_CALLBACK);
		return;
	}
	
	//FIX: added by Yong on 11/13/03
		
	if ( nbr->FailCounter < ZBR_FAIL_THRESHOLD ) {
		/* zheng: no retransmission required in our doc
		if ((ch->ptype() != PT_ZBR)&&(nbr->FailCounter < ZBR_FAIL_THRESHOLD - 1))	//zheng: should try to retransmit the packet
		{
			ch->size() -= IP_HDR_LEN;
		  	Scheduler::instance().schedule(this, p, ((Random::random() % (5 + 10 * nbr->FailCounter)))*(HDR_CMN(p)->size()/dRate * 8));
		}
		else
		*/
			drop(p, DROP_RTR_MAC_CALLBACK);
		nbr->FailCounter ++;
		return;
	}
	//FIX END	
		
	rt = rtable.rt_lookup(ih->daddr());

        //int rt_hops = rt?rt->rt_hops:-1;
        //if (ch->num_forwards() > rt_hops)	//zheng: add -- perform local repair only if the broken link is closer to the destination
        {
	        if (RNType == 1) {
	        	if(rt == 0) {
	        		drop(p, DROP_RTR_MAC_CALLBACK);
	          		return;
	        	}
	        else {
	                rt_down(rt);
	        	
	        	if ((nbr->Rel == 0) || (nbr->Rel == 2)) {
	        		
	        		rt->all_brct_flag = 1;
	        		nbr->NodeFailure = 1;
	        	}
	        	else 
	        		rt->all_brct_flag = 0;	
	        }		
	        }
	        else {
	        	if (rt == 0)
	        	rt = rtable.rt_add(ih->daddr());
	        else
	        	rt_down(rt);	
	        rt->all_brct_flag = 1;
	        nbr->NodeFailure = 1;
	        }				
	        
	        
	        rt->rt_ori_src = ih->saddr();
	        rt->loc_repl_flag = 1;
	        
	        local_rt_repair(rt, p);
	        
	        /*/* if the broken link is closer to the dest than source,
	           attempt a local repair. Otherwise, bring down the route. */
	        
	        
	        /*if (ch->num_forwards() > rt->rt_hops) {
	          local_rt_repair(rt, p); // local repair
	          // retrieve all the packets in the ifq using this link,
	          // queue the packets for which local repair is done,
	          return;
	        }*/
	}
	#else
        {
	    drop(p, DROP_RTR_MAC_CALLBACK);
	    // Do the same thing for other packets in the interface queue using the
	    // broken link -Mahesh
	   while((p = ifqueue->filter(broken_nbr))) {
	   	drop(p, DROP_RTR_MAC_CALLBACK);
	    }
	    nb_delete(broken_nbr);
	}
	#endif // LOCAL REPAIR
	
	//NEW END
	
	#endif // LINK LAYER DETECTION
}

void
ZBR::handle_link_failure(nsaddr_t id) {
	zbr_rt_entry *rt, *rtn;
	for(rt = rtable.head(); rt; rt = rtn) {  // for each rt entry
	   rtn = rt->rt_link.le_next;
	   if ((rt->rt_hops != INFINITY2) && (rt->rt_nexthop == id) /**/ 
	&& (rt->rt_flags == RTF_UP) /**/ ) {
	     // assert (rt->rt_flags == RTF_UP);
	#ifdef ZBRDEBUG
	     fprintf(stderr, "%s(%f): %d\t(%d)\n", __FUNCTION__, CURRENT_TIME, index,
			     rt->rt_nexthop);
	#endif // ZBRDEBUG
	     rt_down(rt);
	   }
	   // remove the lost neighbor from all the precursor lists
	}
}

void
ZBR::local_rt_repair(zbr_rt_entry *rt, Packet *p, bool retry) {
	#ifdef ZBRDEBUG
	  fprintf(stderr,"%s: Dst - %d\n", __FUNCTION__, rt->rt_dst);
	#endif
	  
	//NEW: Yong modified on 10/10/03 for ZigBee routing  
	  // Buffer the packet
	  if (!retry)
	  	rqueue.enque(p);
	  //drop(p, DROP_RTR_MAC_CALLBACK);
	
	  // mark the route as under repair
	  //rt->rt_flags = RTF_IN_REPAIR;
	
	  sendRequest(rt->rt_dst);
	
	//NEW END
	
	  // set up a timer interrupt

	  //
	  // is not properly set
	  double delay;
	  if (!rt->all_brct_flag)
  		delay = 2.0 * (double) HDR_IP(p)->ttl_ * PerHopTime(rt);
	  else
  	  	delay = 2.0 * (double)(2 * NetSystemConfig.Lm) * PerHopTime(rt);
	  //
	  Scheduler::instance().schedule(&lrtimer, retry?p:p->copy(), delay);
}
//设置链路可用
void
ZBR::rt_update(zbr_rt_entry *rt, u_int16_t metric,
	       	nsaddr_t nexthop, double expire_time) {

     rt->rt_hops = metric;
     rt->rt_flags = RTF_UP;
     rt->rt_nexthop = nexthop;
     rt->rt_expire = expire_time;
}
//设置链路不可用
void
ZBR::rt_down(zbr_rt_entry *rt) {
  /*
   *  Make sure that you don't "down" a route more than once.
   */

  if(rt->rt_flags == RTF_DOWN) {
    return;
  }

  // assert (rt->rt_seqno%2); // is the seqno odd?
  rt->rt_last_hop_count = rt->rt_hops;
  rt->rt_hops = INFINITY2;
  rt->rt_flags = RTF_DOWN;
  rt->rt_nexthop = 0;
  rt->rt_expire = 0;

} /* rt_down function */

/*
  Route Handling Functions
*/
//test_for_dy
//链路控制程序
  //决定到底是转还是发起寻找的过程
  //rt routing table 管理的程序

  //这个是在什么情况下会出发这个函数的呢？
void
ZBR::rt_resolve(Packet *p) {
	struct hdr_cmn *ch = HDR_CMN(p); //得到包头
	struct hdr_ip *ih = HDR_IP(p);//得到ip头
	zbr_rt_entry *rt;
	
	
	//NEW: Yong modified on 10/10/03 for ZigBee routing
	nsaddr_t cluster_tree_rt;
	ZBR_Neighbor* nbr;
	//NEW END
	
	/*
	  *  Set the transmit failure callback.  That
	  *  won't change.
	  */
	ch->xmit_failure_ = zbr_rt_failed_callback;
	ch->xmit_failure_data_ = (void*) this;
	
	
	//NEW: Yong modified on 10/10/03 for ZigBee routing
	//ih-daddr 返回的是完整的地址
	rt = rtable.rt_lookup(ih->daddr());
	
	//NEW END
	
	
	 
	//NEW: Yong modified on 10/10/03 for ZigBee routing
	//这个是在判断 路由表中是否有对应的路径  如果有就直接传送
	 if(rt && (rt->rt_flags == RTF_UP)) {
	   	assert(rt->rt_hops != INFINITY2);
		forward(rt, p, ZBR_NO_DELAY);
	 }
	 else {
			
	   	if (RNType == 0) {
			//这个应该是找到下一跳的地址
			cluster_tree_rt = rt_clusterTree(ih->daddr());
			//这个是在邻居表中寻找一次路径
			nbr = nb_lookup (cluster_tree_rt);
					
			//zheng: here must check if  is null or not.
			int nbr_NodeFailure = (nbr)?nbr->NodeFailure:1;		//zheng: treated as failure if the node is not on the neighbor list
	
			//NEW: Yong modified on 11/07/03 for one bug
			//如果路由表中没有这个字段或者路径不存在不能使用的话  并且 邻居表不存在或者邻居表的不能传输的话  那么就直接使用树协议
			if (((rt == 0) || (rt->rt_flags != RTF_UP)) && (nbr_NodeFailure != 1))    {
			//NEW END
				
				tree_forward(cluster_tree_rt, p, ZBR_NO_DELAY);
				return;
	 		}
			else if (nbr_NodeFailure == 1) {
				if (rt == 0) {
					//表示是邻居这个线路是有问题的不能正常使用情况下或者是没有对应的邻居有相应的路由表存在就进行一次请求
					//添加这个路由条目的原因是什么的呢？ 添加一个到目的地址的节点 进行路由请求 这样之后有对应的路由就可以直接发送了 在最上面的一层的判断处
					//会不会之后如果收到了具体的就更新 要不然到了规定的时间就自己清除了？
					//添加的这个ih->daddr 就是这个路径的目的弟子
					//rt->rt_dst = ih->daddr
					rt = rtable.rt_add(ih->daddr());
					rt->all_brct_flag = 1;
					rt->loc_repl_flag = 1;
				}	
				rqueue.enque(p);
				//设置源地址来进行覆盖?
				//为了进行反向的时候使用  千真万确就是为了当获取到了回复的时候 进行反向链接的地址 
				//表示这个是前一跳的地址
				rt->rt_ori_src = ih->saddr();
				//当前没有具体的路径  发起一个请求寻找具体的路径？
				sendRequest(rt->rt_dst);
				return;
				
			}	
			
	 	}
		
		//For RN+:
		
		else	{
		        
			if (rt == 0) {
				//添加这个的目地就是为了下面之后的操作assert不会失败
				rt = rtable.rt_add(ih->daddr());
				//NEW: Yong modified on 10/10/03 for ZigBee routing
				rt->all_brct_flag = 0;
				rt->loc_repl_flag = 0;
				//NEW END
			}	
		
			//If there is no existing routing entry, buffer data firstly. 
	 		rqueue.enque(p);
	
			//If it is not the source node, it must be an agent node, so set up a reverse
			//entry toward the source if there is not an existing one. 
	 		//这表示如果不是源节点的话先建立反向路由
	 		if(ih->saddr() != index) {
	 	
	 			// We make sure that the REVERSE route is in the route table.
	  			zbr_rt_entry *rt0; // rt0 is the reverse route 
	   			rt0 = rtable.rt_lookup(ih->saddr());
	   		
						
				if(rt0 == 0) { /* if not in the route table */
	   			
					// create an entry for the reverse route.
	     				rt0 = rtable.rt_add(ih->saddr());
					rt0->all_brct_flag = 0;
					rt0->loc_repl_flag = 0;
	     				rt0->rt_expire = max(rt0->rt_expire, (CURRENT_TIME + ZBR_REV_ROUTE_LIFE));
					rt_update(rt0, (NetSystemConfig.Lm - myDepth), ch->prev_hop_, max(rt0->rt_expire, (CURRENT_TIME + ZBR_REV_ROUTE_LIFE)) );
	
	   			}//到这为止  反向路由建立完成
	  				
	   		
	     			/* Find out whether any buffered packet can benefit from the 
	      				* reverse route.
	      				* May need some change in the following code - Mahesh 09/11/99
	      				*/
	     			//可能这个反向路由对有的路由包有作用  对buffer的包进行处理
	   				//对于rt0的执行看是否存在或者存在是否为down来确定执行
	     			if (rt0->rt_flags == RTF_UP)
	     			{
	     				assert (rt0->rt_flags == RTF_UP);
	     				Packet *buffered_pkt;
	     				while ((buffered_pkt = rqueue.deque(rt0->rt_dst))) {
		       				if (rt0 && (rt0->rt_flags == RTF_UP)) {
							assert(rt0->rt_hops != INFINITY2);
	         					forward(rt0, buffered_pkt, ZBR_NO_DELAY);
						}
	     				}
	     			}
			}	
	   	
			rt->rt_ori_src = ih->saddr();
			
	  		sendRequest(rt->rt_dst);
	
		
		} 
	   	// End for putting reverse route in rt table
	 }	
	   	
	   	
	 
	//NEW END 

}



//NEW: Yong modified on 10/10/03 for ZigBee routing

// Added by zhu for tree-routing
//给一个目的地址  根据当前节点然后返回下一跳的地址 根据树模型
nsaddr_t
ZBR::rt_clusterTree(nsaddr_t dst) {
	//struct hdr_cmn *ch = HDR_CMN(p);
	//struct hdr_ip *ih = HDR_IP(p);
	
	//// zhu
	u_int32_t	FinalDstNodeID, InitialSrcNodeID, ID;
	int i;
	ZBR_Neighbor*  nb;
	//ZBR_Node*		node;
	
	nsaddr_t NextHop;
	
	FinalDstNodeID = getIpAddrFrLogAddr(dst);	//
	
	if ((FinalDstNodeID < myNodeID) || (FinalDstNodeID >= (myNodeID+1+(ZBR::c_skip(myDepth)*NetSystemConfig.Cm)))) {
		
		NextHop = getIpAddrFrLogAddr(myParentNodeID);	//
		return NextHop;
	}
	
	else {
		if (NetSystemConfig.Cm > 1)
			{
				for (i=0; i < FinalDstNodeID;i++)
					{
						if (i > 0)
						{
							NextHop = getIpAddrFrLogAddr(myNodeID+1+((i-1)*ZBR::c_skip(myDepth)));
							return NextHop;

						}
						else
						{
							//* Error. Impossible situation.
							//error_proc(NOM_DATA_FORWARD_NO_CHILD);
						}
						break;
					}
				}
			else //// zhu: Cm==1
			{
				NextHop = getIpAddrFrLogAddr(myNodeID + 1);
				return NextHop;
			}
		}
}

//// zhu: end */

//NEW END


void
ZBR::rt_purge() {
	zbr_rt_entry *rt, *rtn;
	double now = CURRENT_TIME;
	double delay = 0.0;
	Packet *p;
	Packet *prev;
	
	for(rt = rtable.head(); rt; rt = rtn) {  // for each rt entry
	   rtn = rt->rt_link.le_next;
	   //这个表示的是如果路由表中的链路是可用  但是过期时间已经到了 就全部抛弃
	   if ((rt->rt_flags == RTF_UP) && (rt->rt_expire < now)) {
	   // if a valid route has expired, purge all packets from
	   // send buffer and invalidate the route.
		assert(rt->rt_hops != INFINITY2);
	     while((p = rqueue.deque(rt->rt_dst))) {
	#ifdef ZBRDEBUG
	       fprintf(stderr, "%s: calling drop()\n",
	                       __FUNCTION__);
	#endif // ZBRDEBUG
	       drop(p, DROP_RTR_NO_ROUTE);
	     }
	     rt_down(rt);
	   }
	   //这个表示的是如果路由表中的链路是可用  但是过期时间还没到 就全部遍历一次缓存的buffer 如果有恰当的就进行传输
	   else if (rt->rt_flags == RTF_UP) {
	   // If the route is not expired,
	   // and there are packets in the sendbuffer waiting,
	   // forward them. This should not be needed, but this extra
	   // check does no harm.
	     assert(rt->rt_hops != INFINITY2);
	     while((p = rqueue.deque(rt->rt_dst))) {
	       forward (rt, p, delay);
	       delay += ZBR_ARP_DELAY;
	     }
	   }
	   //查看缓存的buffer 中 是否有对应的目的地址  如果有的话就进行一次路由的请求来进行路由
	   //这里说明链路有问题但是有相应的目的节点就需要进行一次路由路径的请求了
	   else if (rqueue.find(rt->rt_dst)) {
	   // If the route is down and
	   // if there is a packet for this destination waiting in
	   // the sendbuffer, then send out route request. sendRequest
	   // will check whether it is time to really send out request
	   // or not.
	   // This may not be crucial to do it here, as each generated
	   // packet will do a sendRequest anyway.
	
	   //sendRequest(rt->rt_dst);
	
	
	   rqueue.findPacketWithDst(rt->rt_dst, p, prev);
	   struct hdr_cmn *ch = HDR_CMN(p);
	   struct hdr_ip *ih = HDR_IP(p);
	
	      
	//NEW: Yong modified on 10/10/03 for ZigBee routing   
	   //这个地址的设置真的是头大的很啊
	   rt->rt_ori_src = ih->saddr();
	//NEW END
	   
	   sendRequest(rt->rt_dst);
	   
	   }
	  }

}

/*
  Packet Reception Routines
*/

void
ZBR::recv(Packet *p, Handler*) {
	struct hdr_cmn *ch = HDR_CMN(p);//得到包头
	struct hdr_ip *ih = HDR_IP(p);//得到ip头
	
	
	assert(initialized());
	//assert(p->incoming == 0);
	// XXXXX NOTE: use of incoming flag has been depracated; In order to track direction of pkt flow, direction_ in hdr_cmn is used instead. see packet.h for details.
	
	//zheng: we need to populate the neighbor list here.
	//	 note that the real relationship should be set through association.
	//设置一个邻居节点 插入到邻居节点   节点为上一跳的地址
	//只有ch 才有上一条下一跳的么？
	nb_insert(ch->prev_hop_,NEIGHBOR);
	
	//NEW: Yong modified on 11/07/03
	//这里通过设置一跳的地址为可用 之后如果有恰当的话就可以直接使用的了
	ZBR_Neighbor *nbr;
	nbr = nb_lookup(ch->prev_hop_);
	nbr->NodeFailure = 0;
	
	//FIX: Yong modified on 11/13/03
	nbr->FailCounter = 0;
	//FIX END
	
	//NEW END
	//这里就是请求的接受时候 因为是广播 所以为这里
	if(ch->ptype() == PT_ZBR) {//这是处理广播的  就是路由选路的选择
	   ih->ttl_ -= 1;
	   //这个函数的作用是什么？？
	   recvZBR(p);
	   return;
	}
	
	/*
	  *  Must be a packet I'm originating...
	  */
	//表示这个节点包是从我这里开始传出去的
	//这里表示这个几点还是没有变化  转发的次数为0   
	if((ih->saddr() == index) && (ch->num_forwards() == 0)) {
	/*
	  * Add the IP Header
	  */
	   ch->size() += IP_HDR_LEN;
	   // To handle broadcasting
	   if ( (u_int32_t)ih->daddr() != IP_BROADCAST)
	     ih->ttl_ = ZBR_NETWORK_DIAMETER;
	}
	/*
	  *  I received a packet that I sent.  Probably
	  *  a routing loop.
	  */
	//这表示是一个环路 回环了 就丢弃数据包
	else if(ih->saddr() == index) {
	   drop(p, DROP_RTR_ROUTE_LOOP);
	   return;
	}
	/*
	  *  Packet I'm forwarding...
	  */
	else {
	/*
	  *  Check the TTL.  If it is zero, then discard.
	  */
		//节点的条数为0 也表示为0 就是直接丢弃节点包就ok
	   if(--ih->ttl_ == 0) {
	     drop(p, DROP_RTR_TTL);
	     return;
	   }
	}
	
	// Added by Parag Dadhania && John Novatnack to handle broadcasting
	if ( (u_int32_t)ih->daddr() != IP_BROADCAST)
	{
		//不是广播的包进行自己的一次路由表查询 
	   rt_resolve(p);	//不是广播包 进行解析
	}
	else
	   forward((zbr_rt_entry*) 0, p, ZBR_NO_DELAY);	
	   
}

void
ZBR::recvZBR(Packet *p) {
	struct hdr_zbr *ah = HDR_ZBR(p);
	
 assert(HDR_IP (p)->sport() == RT_PORT);//这个表示的是所有的都接受
 assert(HDR_IP (p)->dport() == RT_PORT);
	
	/*
	  * Incoming Packets.
	  */
	switch(ah->ah_type) {
	
	case ZBRTYPE_RREQ:
	   recvRequest(p);
	   break;
	
	case ZBRTYPE_RREP:
	   recvReply(p);
	   break;
	
	default:
	   fprintf(stderr, "Invalid ZBR type (%x)\n", ah->ah_type);
	   exit(1);
	}
}

//在这个函数里面找到了两个sendReply表明是确实收到了最后的目的地址了 需要进行回复
void
ZBR::recvRequest(Packet *p) {
	//printf("in %s my id is :%d dst is:%d\n",__FUNCTION__,index,dst );
	//printf("in %s now my id is :%d my NodeID is %d my depth is %d my ParentNodeID is %d \n",__FUNCTION__,index,myNodeID,myDepth,myParentNodeID );
	
	//struct hdr_cmn *ch = HDR_CMN(p);
	struct hdr_ip *ih = HDR_IP(p);
	struct hdr_zbr_request *rq = HDR_ZBR_REQUEST(p);
	zbr_rt_entry *rt;
	
	//NEW: Yong modified on 10/10/03 for ZigBee routing
	nsaddr_t src_tree_rt;
	nsaddr_t dst_tree_rt;
	//NEW END
	
	  /*
	   * Drop if:
	   *      - I'm the source
	   *      - I recently heard this request.
	   */
	//自己发送的请求包  自己接受到了
	//这个是rq中的地址就是最后的目的地址 和最开始的地址  通过判断
	printf("[ in %s\t,my id is:%d\t"
					"rq src is:%d\t"
					"rq dst is:%d\t"
					"ip src is %d\t"
					"ip dst is %d\t"
					"my NodeID is:%d\t"
					"my parent id is:%d]\n",
					__FUNCTION__,
					index,
					rq->rq_src,
					rq->rq_dst,
					ih->saddr(),
					ih->daddr(),
					myNodeID,
					myParentNodeID);
	  if(rq->rq_src == index) {
	#ifdef ZBRDEBUG
	    fprintf(stderr, "%s: got my own REQUEST\n", __FUNCTION__);
	#endif // ZBRDEBUG
	    Packet::free(p);
	    return;
	  }
	
	  
	//NEW: Yong modified on 10/10/03 for ZigBee routing
	 //这里表示为树路由的进行回复
	 if ((RNType == 0) && (rq->all_brct_flag != 1)) {
	 	//这个表示到请求的源地址 的下一跳的地址
	 	//表示是请求的地址作为树模型来进行传送的
		src_tree_rt = rt_clusterTree(rq->rq_src);
			
		if (rq->loc_repl_flag != 1)
			rq->loc_repl_flag = 0;
	 	//为什么使用这两个进行比较呢？
	 	//这个觉得好巧妙  说不出来味道
	 	//在这一步之前设置了ip为上一跳的地址的
	 	//ih->saddr()=index  通过这里设置了的
		if (src_tree_rt == ih->saddr()) {
	 		//表示当前的节点是目的节点  就发送接收到的请求
	 		//这个是最终的目的地址才能发送回复所以请求的目的地址是这个
	   		if(rq->rq_dst == index) {
	   			//这个只能够一跳一跳的进行回复
	   			//表示回复穿过来的那一跳的节点
				sendReply(rq->rq_src,           // IP Destination
	             		1,                    // Hop Count
	             		index,                // Dest IP Address
	             		ZBR_MY_ROUTE_TIMEOUT,     // Lifetime
	             		rq->rq_timestamp, 0, rq->loc_repl_flag, 0);
							 	
	             		Packet::free(p);
	             	}	  
	 		//感觉这就是像一个回环的一样  需要直接删除掉
	             	//表示按照树形的话  下一跳就是之前的前一跳地址  所以并没什么必要 直接删除就行
			else if ((dst_tree_rt = rt_clusterTree(rq->rq_dst)) == ih->saddr())
				Packet::free(p);
				 
			else {
				//表示进行对应的转发都是改变ip包的操作而不是请求包的操作
				//进行ip包的设置   并进行广播 转为树路由传播
				//进行的是ip包的设置 请求包只是作为一个使用进行查找而已
	 			ih->saddr() = index;
	   			ih->daddr() = IP_BROADCAST; //Question: IP level broadcast packet?
	   			rq->rq_hop_count += 1;
	   			tree_forward(dst_tree_rt, p, ZBR_NO_DELAY);
	   		}	
	   
	   	}
	   	else 
	   		Packet::free(p);
	   	return;
	 } 			
	// NEW END
	
	
	//表示这个广播的请求包已经转发过了 就直接抛弃  不再需要继续的转发了
	if (id_lookup(rq->rq_src, rq->rq_bcast_id)) {
	
	#ifdef ZBRDEBUG
	   fprintf(stderr, "%s: discarding request\n", __FUNCTION__);
	#endif // ZBRDEBUG
	
	   Packet::free(p);
	   return;
	}
	
	/*
	  * Cache the broadcast ID
	  */
	//第一次进行缓存  以后有重复的进来就直接进行抛弃 
	id_insert(rq->rq_src, rq->rq_bcast_id);
	
	
	/*
	  * We are either going to forward the REQUEST or generate a
	  * REPLY. Before we do anything, we make sure that the REVERSE
	  * route is in the route table.
	  */
	zbr_rt_entry *rt0; // rt0 is the reverse route
	//查找有木有这个路由 如果没有就建立读应的反向路由
	   rt0 = rtable.rt_lookup(rq->rq_src);
	
	   //
	   int newrt=0;
	   //
	   if(rt0 == 0) { /* if not in the route table */
	   // create an entry for the reverse route.
	     rt0 = rtable.rt_add(rq->rq_src);
	     
	     //NEW: Yong modified on 10/10/03 for ZigBee routing
	     rt0->all_brct_flag = 0;
	     rt0->loc_repl_flag = 0;
	     //NEW END
	     
	     //
	     newrt=1;
	     //
	   }
	
	   rt0->rt_expire = max(rt0->rt_expire, (CURRENT_TIME + ZBR_REV_ROUTE_LIFE));
	
	
	///NEW: Yong modified on 10/10/03 for ZigBee routing
	   //建立一个反向路由就要对缓存的数据包进行对应的扫描发送
	//反向路由的刷新？
	 if (newrt || rq->loc_repl_flag || (rq->rq_hop_count < rt0->rt_hops)) {
	rt_update(rt0, /*rq->rq_src_seqno, */rq->rq_hop_count, ih->saddr(),
	     	       max(rt0->rt_expire, (CURRENT_TIME + ZBR_REV_ROUTE_LIFE)) );
	//
	// NEW END
	
	     if (rt0->rt_req_timeout > 0.0) {
	     // Reset the soft state and
	     // Set expiry time to CURRENT_TIME + ZBR_ACTIVE_ROUTE_TIMEOUT
	     // This is because route is used in the forward direction,
	     // but only sources get benefited by this change
	       rt0->rt_req_cnt = 0;
	       rt0->rt_req_timeout = 0.0;
	       rt0->rt_req_last_ttl = rq->rq_hop_count;
	       rt0->rt_expire = CURRENT_TIME + ZBR_ACTIVE_ROUTE_TIMEOUT;
	     }
	
	     /* Find out whether any buffered packet can benefit from the
	      * reverse route.
	      * May need some change in the following code - Mahesh 09/11/99
	      */
	     //建立了反向路由  如果有对应的包可以使用那么就进行传播
	     assert (rt0->rt_flags == RTF_UP);
	     Packet *buffered_pkt;
	     while ((buffered_pkt = rqueue.deque(rt0->rt_dst))) {
	       if (rt0 && (rt0->rt_flags == RTF_UP)) {
		assert(rt0->rt_hops != INFINITY2);
	         forward(rt0, buffered_pkt, ZBR_NO_DELAY);
	       }
	     }
	   }
	   // End for putting reverse route in rt table
	
	
	/*
	  * We have taken care of the reverse route stuff.
	  * Now see whether we can send a route reply.
	  */
	
	rt = rtable.rt_lookup(rq->rq_dst);
	
	// First check if I am the destination ..
	//如果请求的最终目的地址就是等于当前节点的地址的话  那么就进行反馈  
	if(rq->rq_dst == index) {
	
	#ifdef ZBRDEBUG
	   fprintf(stderr, "%d - %s: destination sending reply\n",
	                   index, __FUNCTION__);
	#endif // ZBRDEBUG
	
	
	   // Just to be safe, I use the max. Somebody may have
	   // incremented the dst seqno.
	
	   //这个就是最开始的发送位置
	   sendReply(rq->rq_src,           // IP Destination
	             1,                    // Hop Count
	             index,                // Dest IP Address
	             ZBR_MY_ROUTE_TIMEOUT,     // Lifetime
	             rq->rq_timestamp,
	             rq->all_brct_flag, rq->loc_repl_flag, 0);    // timestamp
	
	   Packet::free(p);
	}
	
	/*
	  * Can't reply. So forward the  Route Request
	  */
	else {
		//相当于这里不能进行回复 就进行对应的广播转发
		//这里设置index就相当于说这个是一个前一跳  如果你没有对应的路径 你可以从中找到并且进行设置
		//在这里进行forwaord 就不再进行request了
	   ih->saddr() = index;
	   ih->daddr() = IP_BROADCAST;
	   rq->rq_hop_count += 1;
	   // Maximum sequence number seen en route
	   forward((zbr_rt_entry*) 0, p, ZBR_DELAY);
	}

}


//请求包的目的地址主要为index  应该是为了之后的判断进行从新转发的需要
void
ZBR::recvReply(Packet *p) {
	//struct hdr_cmn *ch = HDR_CMN(p);
	struct hdr_ip *ih = HDR_IP(p);
	struct hdr_zbr_reply *rp = HDR_ZBR_REPLY(p);
	zbr_rt_entry *rt;
	char suppress_reply = 0;
	double delay = 0.0;
	printf("[ in %s\t,my id is:%d\t"
					"rq src is:%d\t"
					"rq dst is:%d\t"
					"ip src is %d\t"
					"ip dst is %d\t"
					"my NodeID is:%d\t"
					"my parent id is:%d]\n",
					__FUNCTION__,
					index,
					rp->rp_src,
					rp->rp_dst,
					ih->saddr(),
					ih->daddr(),
					myNodeID,
					myParentNodeID);
	#ifdef ZBRDEBUG
	fprintf(stderr, "%d - %s: received a REPLY\n", index, __FUNCTION__);
	#endif // ZBRDEBUG
	
	
	/*
	  *  Got a reply. So reset the "soft state" maintained for
	  *  route requests in the request table. We don't really have
	  *  have a separate request table. It is just a part of the
	  *  routing table itself.
	  */
	// Note that rp_dst is the dest of the data packets, not the
	// the dest of the reply, which is the src of the data packets.
	
	//NEW: Yong modified on 10/10/03 for ZigBee routing
	
	rt = rtable.rt_lookup(rp->rp_dst);
	
	  if ((RNType == 0) && (rp->all_brct_flag != 1)) {
	     	
	     	//NEW: Yong modified on 11/07/03 for one bug
	     	if(rt && (rt->rt_flags == RTF_UP))
	     		rt_down(rt);
	     	//NEW END	
	     	
	     	if (ih->daddr() == index)
	     		Packet::free(p);
	     	else {
	     		rp->rp_hop_count += 1;
	     		rp->rp_src = index;  //question: why? //表示在请求包中就进行了改变  在ip包中是最后的目的地址
	     		//这个表示的是去下一跳的地址  通过树的协议 接收到的路由说明了下一跳的地址在哪里
	        	tree_forward(rt_clusterTree(ih->daddr()), p, ZBR_NO_DELAY);
	        }	
	        return;	
	  }
	
	//NEW END
	
	
	/*
	  *  If I don't have a rt entry to this host... adding
	  */
	//
	int newrt=0;
	//
	if(rt == 0) {
		//？
	   rt = rtable.rt_add(rp->rp_dst);
	   
	//NEW: Yong modified on 10/10/03 for ZigBee routing
	   rt->all_brct_flag = 0;
	   rt->loc_repl_flag = 0;
	//NEW END
	   
	   newrt=1;
	}
	
	/*
	  * Add a forward route table entry... here I am following
	  * Perkins-Royer ZBR paper almost literally - SRD 5/99
	  */
	
	//NEW: Yong modified on 10/10/03 for ZigBee routing
	//
	 if (newrt || rp->loc_repl_flag || (rt->rt_hops >= rp->rp_hop_count)) { // shorter
	//
	//NEW END
	
	  // Update the rt entry
	  rt_update(rt, rp->rp_hop_count,
			rp->rp_src, CURRENT_TIME + rp->rp_lifetime);
	
	  // reset the soft state
	  rt->rt_req_cnt = 0;
	  rt->rt_req_timeout = 0.0;
	  rt->rt_req_last_ttl = rp->rp_hop_count;
	
	if (ih->daddr() == index) { // If I am the original source
	  // Update the route discovery latency statistics
	  // rp->rp_timestamp is the time of request origination
	
	    rt->rt_disc_latency[rt->hist_indx] = (CURRENT_TIME - rp->rp_timestamp)
	                                         / (double) rp->rp_hop_count;
	    // increment indx for next time
	    rt->hist_indx = (rt->hist_indx + 1) % MAX_HISTORY;
	    
	//NEW: Yong modified on 10/10/03 for ZigBee routing
	    //这个就是两个index相等的情况
	    //找到设置对应的rt_ori_src 的地方
	    //2017/1/2 表明第一个参数是发送的地址也就是本节点的地址  因为会判断是不是自己发送的
	    //第三个为下一跳的地址能够直接传送过去的地址
	    //这个是不是代表两个收到回复后的回馈呢？
	    //这应该是一种对收到rrep的反馈  
	    //后面的才是决定是转发还是如果自己就是最后的目的地址就接受了
	    if ((rt->rt_ori_src != index) && (rp->grat_flag !=1)) {
			sendReply(rp->rp_dst,           // IP Destination    //第一个才是最后的目的地址 //这个地址就是之前传过来的index地址
	             		1,                    // Hop Count
	             		rt->rt_ori_src,                // Dest IP Address   //表示第三个只是一个下一跳的地址而已 
	             		ZBR_MY_ROUTE_TIMEOUT,     // Lifetime
	             		rp->rp_timestamp, rp->all_brct_flag, rp->loc_repl_flag, 1);
			delay = 0.01 + 0.01 * Random::uniform();
	    }			
		   			
	//NEW END
	  }
	
	  /*
	   * Send all packets queued in the sendbuffer destined for
	   * this destination.
	   * XXX - observe the "second" use of p.
	   */
	  Packet *buf_pkt;
	  //不存在的路由  建立了的话就进行使用  把暂时缓存的包都进行发送
	  while((buf_pkt = rqueue.deque(rt->rt_dst))) {
	    if ((rt->rt_hops != INFINITY2)/**/ && (rt->rt_flags == RTF_UP) /**/) {
	          // assert (rt->rt_flags == RTF_UP);
	    // Delay them a little to help ARP. Otherwise ARP
	    // may drop packets. -SRD 5/23/99
	      forward(rt, buf_pkt, delay);
	      delay += ZBR_ARP_DELAY;
	    }
	  }
	}
	else {
		//表示的是阻止生长的意思  不准虚接受？
	  suppress_reply = 1;
	}
	
	/*
	  * If reply is for me, discard it.
	  */
	//表示如果我就是最后的接受者 那就给我要不发就进行转发
	if(ih->daddr() == index || suppress_reply) {
	   Packet::free(p);
	}
	/*
	  * Otherwise, forward the Route Reply.
	  */
	else {
		//这就是进行对应的转发 如果链路正常的话那么就进行转发  需要修改回复包的对应内容 就是回复包的源地址该为当地地址就行了
	// Find the rt entry
	zbr_rt_entry *rt0 = rtable.rt_lookup(ih->daddr());
	   // If the rt is up, forward
	   if(rt0 && (rt0->rt_hops != INFINITY2) /**/ && (rt0->rt_flags == RTF_UP) /**/) {
	        // assert (rt0->rt_flags == RTF_UP);
	     rp->rp_hop_count += 1;
	     rp->rp_src = index;
	     forward(rt0, p, ZBR_NO_DELAY);
	   }
	   else {
	   // I don't know how to forward .. drop the reply.
	#ifdef ZBRDEBUG
	     fprintf(stderr, "%s: dropping Route Reply\n", __FUNCTION__);
	#endif // ZBRDEBUG
	     drop(p, DROP_RTR_NO_ROUTE);
	   }
	}
}

/*
   Packet Transmission Routines
*/

void
ZBR::forward(zbr_rt_entry *rt, Packet *p, double delay) {
	struct hdr_cmn *ch = HDR_CMN(p);
	struct hdr_ip *ih = HDR_IP(p);
	
	if(ih->ttl_ == 0) {
	
	#ifdef ZBRDEBUG
	  fprintf(stderr, "%s: calling drop()\n", __PRETTY_FUNCTION__);
	#endif // ZBRDEBUG
	
	  drop(p, DROP_RTR_TTL);
	  return;
	}
	//在这里的here代表什么意思呢？没找到定义
	//这个表是的是如果是一个广播包的话或者  就是发给我自己的包我就收下  
	if (ch->ptype() != PT_ZBR && ch->direction() == hdr_cmn::UP &&
		((u_int32_t)ih->daddr() == IP_BROADCAST)
			|| ((u_int32_t)ih->daddr() == here_.addr_)) {
		dmux_->recv(p,0);
		return;
 	}

	//NEW: Yong modified on 10/10/03 for ZigBee routing
	//这个表示的是如果不能当前节点接受的话  就需要进行转发需要在各个节点上进行设置  前置的节点 前置的跳
	 ch->prev_hop_ = index;
	 //NEW END
	
	
	if (rt) {
	   assert(rt->rt_flags == RTF_UP);
	   rt->rt_expire = CURRENT_TIME + ZBR_ACTIVE_ROUTE_TIMEOUT;
	   //这里表示如果在路由表中存在对应的链路 那么就设置节点包的下一跳的地址为当前链路信息额下一跳就可
	   ch->next_hop_ = rt->rt_nexthop;
	
	   //NEW: Yong modified on 10/10/03 for ZigBee routing
	   ch->addr_type() = NS_AF_ILINK;
	   //ch->addr_type() = NS_AF_INET;
	   //NEW END
		//这里为什么是向下的呢？还有几种方法？
	   ch->direction() = hdr_cmn::DOWN;       //important: change the packet's direction
	}
	else { // if it is a broadcast packet
	   // assert(ch->ptype() == PT_ZBR); // maybe a diff pkt type like gaf
	   assert(ih->daddr() == (nsaddr_t) IP_BROADCAST);
	   ch->addr_type() = NS_AF_NONE;
	   ch->direction() = hdr_cmn::DOWN;       //important: change the packet's direction
	}
	//关于是否为广播包的使用和理解 还有延时的使用
	if (ih->daddr() == (nsaddr_t) IP_BROADCAST) {
	// If it is a broadcast packet
	   assert(rt == 0);
	   /*
	    *  Jitter the sending of broadcast packets by 10ms
	    */
	   Scheduler::instance().schedule(target_, p,
	      				   0.01 * Random::uniform());
	}
	else { // Not a broadcast packet
	   if(delay > 0.0) {
	     Scheduler::instance().schedule(target_, p, delay);
	   }
	   else {
	   // Not a broadcast packet, no delay, send immediately
	    
	
	     Scheduler::instance().schedule(target_, p, 0.);
	   }
	}

}

//NEW: Yong modified on 10/10/03 for ZigBee routing
void
ZBR::tree_forward(nsaddr_t tree_rt, Packet *p, double delay) {
	struct hdr_cmn *ch = HDR_CMN(p);
	struct hdr_ip *ih = HDR_IP(p);
	
	if(ih->ttl_ == 0) {
	
	#ifdef ZBRDEBUG
	  fprintf(stderr, "%s: calling drop()\n", __PRETTY_FUNCTION__);
	#endif // ZBRDEBUG
	
	  drop(p, DROP_RTR_TTL);
	  return;
	}
	
	ch->next_hop_ = tree_rt;
	ch->prev_hop_ = index;
	ch->addr_type() = NS_AF_ILINK;
	//ch->addr_type() = NS_AF_INET;
	
	ch->direction() = hdr_cmn::DOWN;       //important: change the packet's direction
	
	Scheduler::instance().schedule(target_, p, 0.);	//delay=0
}

//NEW END


//对于请求包的设置 难道没有区分树和其他模型？
void
ZBR::sendRequest(nsaddr_t dst) {
	printf("[ in %s\t"
				"my id is:%d\t"
				"dst is:%d\t"
				"my NodeID is:%d\t"
				"time is %d]\n",
				__FUNCTION__,
				index,
				dst,
				myNodeID,
				CURRENT_TIME);
	// Allocate a RREQ packet
	Packet *p = Packet::alloc();
	struct hdr_cmn *ch = HDR_CMN(p);
	struct hdr_ip *ih = HDR_IP(p);
	struct hdr_zbr_request *rq = HDR_ZBR_REQUEST(p);
	zbr_rt_entry *rt = rtable.rt_lookup(dst);
	//在之前进行了一次添加  所以说就一定能在路由表里面找到对应的
	//在之前默认添加一个路径的时候是默认为down的
	assert(rt);
	
	/*
	  *  Rate limit sending of Route Requests. We are very conservative
	  *  about sending out route requests.
	  */
	//这里应该是添加一些控制的要求
	//就是因为没有对应的路径  所以为down 所以需要进行判断
	if (rt->rt_flags == RTF_UP) {
	   assert(rt->rt_hops != INFINITY2);
	   Packet::free((Packet *)p);
	   return;
	}
	
	if (rt->rt_req_timeout > CURRENT_TIME) {
	   Packet::free((Packet *)p);
	   return;
	}
	
	// rt_req_cnt is the no. of times we did network-wide broadcast
	// ZBR_RREQ_RETRIES is the maximum number we will allow before
	// going to a long timeout.
	//这个表示的是我们最多可以进行广播的次数  如果次数超过了 自动的在表中进行查找删除
	if (rt->rt_req_cnt > ZBR_RREQ_RETRIES) {
	   rt->rt_req_timeout = CURRENT_TIME + ZBR_MAX_RREQ_TIMEOUT;
	   rt->rt_req_cnt = 0;
	Packet *buf_pkt;
	//在超时包中删除 含有对应节点的包
	   while ((buf_pkt = rqueue.deque(rt->rt_dst))) {
	       drop(buf_pkt, DROP_RTR_NO_ROUTE);
	   }
	   Packet::free((Packet *)p);
	   return;
	}
	
	#ifdef ZBRDEBUG
	   fprintf(stderr, "(%2d) - %2d sending Route Request, dst: %d\n",
	                    ++route_request, index, rt->rt_dst);
	#endif // ZBRDEBUG
	
	// Determine the TTL to be used this time.
	// Dynamic TTL evaluation - SRD
	//在这里是决定这个请求传播的范围可以加入具体的方法进行改进 ！！！！！ 
	//进行请求的范围就是寻找最佳路径的范围
	rt->rt_req_last_ttl = max(rt->rt_req_last_ttl,rt->rt_last_hop_count);
	
	if (0 == rt->rt_req_last_ttl) {
	// first time query broadcast
	   ih->ttl_ = ZBR_TTL_START;
	}
	else {
	// Expanding ring search.
	   if (rt->rt_req_last_ttl < ZBR_TTL_THRESHOLD)
	     ih->ttl_ = rt->rt_req_last_ttl + ZBR_TTL_INCREMENT;
	   else {
	   // network-wide broadcast
	     ih->ttl_ = ZBR_NETWORK_DIAMETER;
	     rt->rt_req_cnt += 1;
	   }
	}
	
	// remember the TTL used  for the next time
	rt->rt_req_last_ttl = ih->ttl_;
	
	// PerHopTime is the roundtrip time per hop for route requests.
	// The factor 2.0 is just to be safe .. SRD 5/22/99
	// Also note that we are making timeouts to be larger if we have
	// done network wide broadcast before.
	
	rt->rt_req_timeout = 2.0 * (double) ih->ttl_ * PerHopTime(rt);
	if (rt->rt_req_cnt > 0)
	   rt->rt_req_timeout *= rt->rt_req_cnt;
	rt->rt_req_timeout += CURRENT_TIME;
	
	// Don't let the timeout to be too large, however .. SRD 6/8/99
	if (rt->rt_req_timeout > CURRENT_TIME + ZBR_MAX_RREQ_TIMEOUT)
	   rt->rt_req_timeout = CURRENT_TIME + ZBR_MAX_RREQ_TIMEOUT;
	rt->rt_expire = 0;
	
	#ifdef ZBRDEBUG
	fprintf(stderr, "(%2d) - %2d sending Route Request, dst: %d, tout %f ms\n",
		         ++route_request,
			 index, rt->rt_dst,
			 rt->rt_req_timeout - CURRENT_TIME);
	#endif	// ZBRDEBUG
	
	
	// Fill out the RREQ packet
	// ch->uid() = 0;
	//进行数据包的填充
	ch->ptype() = PT_ZBR;
	ch->size() = IP_HDR_LEN + rq->size();
	ch->iface() = -2;
	ch->error() = 0;
	ch->addr_type() = NS_AF_NONE;
	//表示的跳树 上一跳地址
	ch->prev_hop_ = index;          // ZBR hack
	//在这里ip表示为广播的包
	ih->saddr() = index;
	ih->daddr() = IP_BROADCAST;
	ih->sport() = RT_PORT;
	ih->dport() = RT_PORT;
	
	
	
	// Fill up some more fields.
	//在这里填写具体的目的地址在哪儿
	rq->rq_type = ZBRTYPE_RREQ;
	rq->rq_hop_count = 1;
	rq->rq_bcast_id = bid++;
	rq->rq_dst = dst;
			
	//NEW: Yong modified on 10/10/03 for ZigBee routing
	
	 if (rt->all_brct_flag == 1)
	 	rq->all_brct_flag = 1;
	 else
	 	rq->all_brct_flag = 0;
		
	 if (rt->loc_repl_flag == 1)
	 	rq->loc_repl_flag = 1;
	 else
	 	rq->loc_repl_flag = 0;
	//NEW END	
	//请求包的 请求源地址就是这个节点的当前地址
	 //相当于每次发送的请求包的请求的源地址都是本地地址  只是目的地址没有变化而已
	rq->rq_src = index;
	rq->rq_timestamp = CURRENT_TIME;
	
	
	Scheduler::instance().schedule(target_, p, 0.);
	
	//NEW: Yong modified on 10/10/03 for ZigBee routing
	 id_insert(rq->rq_src, rq->rq_bcast_id);
	//NEW END

}
//设置具体的包头节点信息 并发送
//NEW: Yong modified on 10/10/03 for ZigBee routing
//是否rrep只是需要进行回去的路线查找而不需要进行路由的寻路了？
void
ZBR::sendReply(nsaddr_t ipdst, u_int32_t hop_count, nsaddr_t rpdst,/*代表这个是收到最终目的地址节点的地址 作为发送的其实节点*/
                u_int32_t lifetime, double timestamp, 
                unsigned int allbrctflag, unsigned int locreplflag, unsigned int grat) {
//NEW END
	printf("[ in %s\t,my id is:%d\t"
					"rq src is:%d\t"
					"rq dst is:%d\t"
					"ip src is %d\t"
					"ip dst is %d\t"
					"my NodeID is:%d\t"
					"my parent id is:%d]\n",
					__FUNCTION__,
					index,
					index,
					rpdst,
					index,
					ipdst,
					myNodeID,
					myParentNodeID);
	Packet *p = Packet::alloc();
	struct hdr_cmn *ch = HDR_CMN(p);
	struct hdr_ip *ih = HDR_IP(p);
	struct hdr_zbr_reply *rp = HDR_ZBR_REPLY(p);
	//之前进行了添加所以说一定能找到对应的
	zbr_rt_entry *rt = rtable.rt_lookup(ipdst);
	
	//NEW: Yong modified on 10/10/03 for ZigBee routing
	
	nsaddr_t rt_entry;
	//这个就是为了获得下一跳的地址
	if ((RNType == 0) && (allbrctflag != 1)) 
		//通过树形找到下一跳的地址
		rt_entry = rt_clusterTree(ipdst);
	else 
		rt_entry = rt->rt_nexthop;
	
	
	//NEW: END	
	
	#ifdef ZBRDEBUG
	fprintf(stderr, "sending Reply from %d at %.2f\n", index, 
	Scheduler::instance().clock());
	#endif // ZBRDEBUG
	//assert(rt);	//zheng: no longer true in ZBR
	
	rp->rp_type = ZBRTYPE_RREP;
	//rp->rp_flags = 0x00;
	rp->rp_hop_count = hop_count;
	//源地址能理解  但是 目的地址为什么不是这个呢？
	//对于这两个地址的理解深入点
	//有两个rpdst==index??
	//对于源地址都是一样的 就只有目的地址不一样而已   请求包的目的地址为第三个 ip包的目的地址为第一个
	rp->rp_dst = rpdst;
	rp->rp_src = index;
	rp->rp_lifetime = lifetime;
	rp->rp_timestamp = timestamp;
	
	//NEW: Yong modified on 10/10/03 for ZigBee routing
	 rp->all_brct_flag = allbrctflag;
	 rp->loc_repl_flag = locreplflag;
	 rp->grat_flag = grat;
	//NEW END
	
	// ch->uid() = 0;
	ch->ptype() = PT_ZBR;
	ch->size() = IP_HDR_LEN + rp->size();
	ch->iface() = -2;
	ch->error() = 0;
	
	 ch->addr_type() = NS_AF_ILINK;
	// ch->addr_type() = NS_AF_INET;
	
	//NEW: Yong modified on 10/10/03 for ZigBee routing
	 
	 ch->next_hop_ = rt_entry;
	 //ch->next_hop_ = rt->rt_nexthop;
	//NEW END
	
	ch->prev_hop_ = index;          // ZBR hack
	ch->direction() = hdr_cmn::DOWN;
	//这个ipdst表示的是最后的地址的么？
	ih->saddr() = index;
	ih->daddr() = ipdst;
	//代表这个地址为发送待待接受的地址 这个并不是最终的发送请求的源节点的地址
	ih->sport() = RT_PORT;
	ih->dport() = RT_PORT;
	ih->ttl_ = ZBR_NETWORK_DIAMETER;
	
	Scheduler::instance().schedule(target_, p, 0.0003);

}

/*
   Neighbor Management Functions
*/
//以下是对邻居节点的 管理 插入  删除   合并 查找
void
ZBR::sscs_nb_insert(nsaddr_t id, unsigned char Rel)
{
	ZBR_Neighbor *nb;

	nb = nb_lookup(id);
	if(nb)	//update relationship
		nb->Rel = Rel;
	else	//add a new neighbor
		nb_insert(id,Rel);
}

void
ZBR::nb_insert(nsaddr_t id, unsigned char Rel) {

	ZBR_Neighbor *nb;
	//
	nb = nb_lookup(id);
	if(nb) return;
	//
	
	nb = new ZBR_Neighbor(id);
	assert(nb);
	nb->nb_expire = CURRENT_TIME +
	                (1.5 * ZBR_ALLOWED_HELLO_LOSS * ZBR_HELLO_INTERVAL);
	//// added by zhu
	nb->Rel = Rel;
	//// end zhu
	
	//FIX: added by Yong on 11/13/03
	nb->NodeFailure = 0;
	nb->FailCounter = 0;
	//FIX END
	 
	LIST_INSERT_HEAD(&nbhead, nb, nb_link);
}


ZBR_Neighbor*
ZBR::nb_lookup(nsaddr_t id) {
	ZBR_Neighbor *nb = nbhead.lh_first;
	
	for(; nb; nb = nb->nb_link.le_next) {
	   if(nb->nb_addr == id) break;
	}
	return nb;
}


/*
* Called when we receive *explicit* notification that a Neighbor
* is no longer reachable.
*/
void
ZBR::nb_delete(nsaddr_t id) {
	ZBR_Neighbor *nb = nbhead.lh_first;
	
	log_link_del(id);
	
	for(; nb; nb = nb->nb_link.le_next) {
	   if(nb->nb_addr == id) {
	     LIST_REMOVE(nb,nb_link);
	     delete nb;
	     break;
	   }
	}
	
	handle_link_failure(id);
	
}


/*
* Purges all timed-out Neighbor Entries - runs every
* HELLO_INTERVAL * 1.5 seconds.
*/
void
ZBR::nb_purge() {
	ZBR_Neighbor *nb = nbhead.lh_first;
	ZBR_Neighbor *nbn;
	double now = CURRENT_TIME;
	
	for(; nb; nb = nbn) {
	   nbn = nb->nb_link.le_next;
	   if(nb->nb_expire <= now) {
	     nb_delete(nb->nb_addr);
	   }
	}
	
}

//NEW: Yong and Zhu modified for ZigBee routing

// Return Cskip value based on Cm and Lm.  
//计算每一层的客使用节点的偏移量
unsigned int 
ZBR::c_skip(unsigned int Li) 
{ 
	double Cm_sum = 0, Cm_mul = 1; 
	unsigned int cskip;
	
	for (int k=0; k<=Li; k++)
	{
		Cm_sum = Cm_sum + pow(NetSystemConfig.Cm, k);
		Cm_mul = Cm_mul * NetSystemConfig.Cm;
	}
		
	cskip = (int)((NetSystemConfig.BlockSize - Cm_sum)/Cm_mul);
		return cskip; 
}

//NEW END

//calculate the block size from Cm and Lm (assume full block)
//这是计算全部能使用为节点个数
unsigned int
ZBR::Cskip_BSize(void)
{
    int Bsize = 1;
    int cb = 1;
    int i;
    for (i = 0; i < NetSystemConfig.Lm; i++)
    {
        cb *= NetSystemConfig.Cm;
        Bsize += cb;
    }

    return Bsize;
}

//

static const int abr_verbose = 0;
void
ZBR::log_link_del(nsaddr_t dst)
{
        static int link_del = 0;

        if(! logtarget || ! abr_verbose) return;

        /*
         *  If "god" thinks that these two nodes are still
         *  reachable then this is an erroneous deletion.
         */
        sprintf(logtarget->pt_->buffer(),
                "A %.9f _%d_ deleting LL hop to %d (delete %d is %s)",
                CURRENT_TIME,
                index,
                dst,
                ++link_del,
                God::instance()->hops(index, dst) != 1 ? "VALID" : 
"INVALID");
        logtarget->pt_->dump();
}


void
ZBR::log_link_broke(Packet *p)
{
        static int link_broke = 0;
        struct hdr_cmn *ch = HDR_CMN(p);

        if(! logtarget || ! abr_verbose) return;

        sprintf(logtarget->pt_->buffer(),"A %.9f _%d_ LL unable to deliver packet %d to %d (%d) (reason = %d, ifqlen = %d)",
                CURRENT_TIME,
                index,
                ch->uid_,
                ch->next_hop_,
                ++link_broke,
                ch->xmit_reason_,
                ifqueue->length());
	logtarget->pt_->dump();
}

void
ZBR::log_link_kept(nsaddr_t dst)
{
        static int link_kept = 0;

        if(! logtarget || ! abr_verbose) return;


        /*
         *  If "god" thinks that these two nodes are now
         *  unreachable, then we are erroneously keeping
         *  a bad route.
         */
        sprintf(logtarget->pt_->buffer(),
                "A %.9f _%d_ keeping LL hop to %d (keep %d is %s)",
                CURRENT_TIME,
                index,
                dst,
                ++link_kept,
                God::instance()->hops(index, dst) == 1 ? "VALID" : 
"INVALID");
        logtarget->pt_->dump();
}

//end of file: zbr.cc

