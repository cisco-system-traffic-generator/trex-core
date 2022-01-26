# **FreeBSD TCP Integration**


## **Files**

`src/44bsd/` has integrated code from 4.4BSD-Lite source and flow-based management code.
The integrated code has been changed a lot from the original and bound strictly to the flow.
To add FreeBSD TCP source to it, `src/44bsd/netinet/` added independantly.

  - added header files:
    - `sys_inet.h`: definitions from system and netinet headers
    - `tcp_mbuf.h`: `struct mbuf` and **required functions**
    - `tcp_socket.h`: `struct socket` and **required funtions**
  - imported header files:
    - `tcp.h`:
    - `tcp_debug.h`:
    - `tcp_seq.h`:
    - `tcp_fsm.h`:
    - `tcp_timer.h`: `struct tcp_timer`
    - `tcp_var.h`: `struct tcpcb`, `struct tcpstat` and **required functions**
  - `tcp_output.c`:
  - `tcp_input.c`:
  - `tcp_sack.c`:
  - `tcp_debug.c`:
  - `tcp_timer.c`:
  - `tcp_subr.c`:

### _Congestion Control Files_
  - `cc/`
    - `cc/cc.h`: `struct cc_algo` and `struct cc_var`
    - `cc_newreno.c`/`cc_newreno.h`:
    - `cc_cubic.c`/`cc_cubic.h`:


## **Guide**

To use this FreeBSD TCP stack integration files properly, please read this guide carefully. There are lots of provided functions and required callback functions should be implemented. And several shared data structures are defined to be used in the TCP stack.

### _**`struct mbuf`**_
It is used to save TCP/IP packet. In the integrated code, it is just used for the type reference only. So, DPDK `struct rte_mbuf` can be used by type casting of this. But, in addition, the following callback functions should be provided properly. They are declared at `netinet/tcp_mbuf.h`.
  - `void m_adj_fix(struct mbuf *m, int req_len, int l7_len);`: trim head data by `req_len`. `l7_len` can be used for `mbuf` sanity check.
  - `void m_trim(struct mbuf *m, int req_len);`: trim tail data by `req_len`.
  - `void m_freem(struct mbuf *m);`: release `mbuf`

#### _**`tcp_int_output()`**_
The `mbuf` would be handled by TCP packet handling functions also. The outbound packets will be handled when you call `tcp_int_output()` function.
  - `int tcp_int_output(struct tcpcb *tp);`
    - by default, `tcp_output(tp)` would be called.

The default `tcp_output()` will call following functions and you should implement them and handle the packet `mbuf` properly.
  - `int tcp_build_cpkt(struct tcpcb *tp, uint16_t hdrlen, struct tcp_pkt *pkt);`
  - `int tcp_build_dpkt(struct tcpcb *tp, uint32_t off, uint32_t len, uint16_t hdrlen, struct tcp_pkt *pkt);`
    - `off`: data offset in the socket buffer to build a packet
    - `len`: data length to build a packet
    - `hdrlen`: IP + TCP header length (including TCP option length)
    - `pkt`: to get the built packet information. see `struct tcp_pkt` in `tcp_var.h`.
      - `struct mbuf *m_buf`: return built packet `mbuf` (allocated)
      - `struct tcphdr *lpTcp`: return tcp header location in the packet
      - `uint16_t m_optlen`: required TCP option length
    - should return 0 for success, other values for failure.
    - _`trex-core` could use `tcp_build_cpkt` and `tcp_build_dpkt` as is._
  - `int tcp_ip_output(struct tcpcb *tp, struct mbuf *m);`
    - should return 0 for success
    - _`trex-core` could use `CTcpIOCb::on_tx()`._

#### _**`tcp_int_respond()`**_
In addition, internal `tcp_respond()` will call `tcp_output()`'s callback functions also. This function will generate a control packet (without data) for the response. The interface function `tcp_int_respond()` is provided for your usage.
  - `void tcp_int_respond(struct tcpcb *tp, tcp_seq ack, tcp_seq seq, int flags);`
    - `ack`: TCP acknowlegement number in the response packet
    - `seq`: TCP sequence number in the response packet
    - `flags`: TCP flags in the response packet

#### _**`tcp_int_input()`**_
For inbound packet handling, you should call `tcp_int_input()` and implement `tcp_reass()` and several socket callback functions.
  - `void tcp_int_input(struct tcpcb *tp, struct mbuf *m, struct tcphdr *th, int toff, int tlen, uint8_t iptos);`
    - `m`: buffer includes TCP header and data
    - `th`: TCP header
    - `toff`: TCP data offset in the `m` buffer
    - `tlen`: TCP data length
    - `iptos`: IP TOS value for ECN processing
    - _`trex-core` could replace `tcp_flow_input()` with this._

### _**`struct socket`**_
`struct socket` is used for the user interface handling. The user data can be kept in the socket buffer inside, `so_snd` and `so_rcv`. All members in this type will be referred and updated by the TCP stack also. So, there are many callback functions for the socket. These are declared at `netinet/tcp_socket.h`

#### _buffer handling callbacks_
  - `void sbappend(struct sockbuf *sb, struct mbuf *m, int flags, struct socket *so);`
    - for `so_rcv`, append data from `m` to the socket buffer.
    - `flags`: would be 0, ignore it.
    - `so`: required to generate socket event
  - `void sbdrop(struct sockbuf *sb, int len, struct socket *so);`
    - for `so_snd`, drop data by `len` from the socket buffer.
    - `so`: required to generate socket event

#### _socket event callbacks_
  - `void sorwakeup(struct socket *so);`
  - `void sowwakeup(struct socket *so);`
  - `void soisconnected(struct socket *so);`
  - `void soisdisconnected(struct socket *so);`
  - `void socantrcvmore(struct socket *so);`
    - set SBS_CANTRCVMORE bit to so_rcv.sb_state to drop the received `mbuf` earlier.

### _**TCP Timer**_
For the TCP Timer handling, `tcp_handle_timers()` should be called by the external timer event. 
  - `void tcp_handle_timers(struct tcpcb *tp);`

In addition, following callback function should be provided to reduce the `tcp_handle_timers()` calls during TCPS_TIME_WAIT state. This function may change the next external timer event interval. It could do nothing also when there is no performance issue.
  - `void tcp_timer_reset(struct tcpcb *tp, uint32_t msec);`

In FreeBSD, the system `ticks` and `hz` are used for the TCP time base. `ticks` used to get the current tick value for TCP time calculation. It can be referred to `now_tick` pointer in the `tcp_timer`. The `*now_tick` should be updated by `hz` resolution. Following functions are provided to convert ticks to/from milli-seconds. They are declared at `netinet/tcp_timer.h`.
  - `uint32_t tcp_timer_msec_to_ticks(uint32_t msec);`
  - `uint32_t tcp_timer_ticks_to_msec(uint32_t tick);`

### _**Congestion Control**_
FreeBSD supports several congestion control algorithms. Following alogrithms are imported and can be activated by given function block `struct cc_algo`.
  - newreno: `struct cc_algo newreno_cc_algo;`
  - qubic: `struct cc_algo cubic_cc_algo;`

The congestion control algorithm requires also `struct cc_var` to handle the algorithm per flow. They are declared at `netinet/cc/cc.h`.

### _**TCP Reassembly**_
The TCP reassembly feature in `netinet/tcp_reass.c` is not integrated for the external efficient code. So, it is excluded for this integration and you should implement optimized `tcp_reass()` callback function.
  - `int tcp_reass(struct tcpcb *tp, struct tcphdr *th, tcp_seq *seq_start, int *tlenp, struct mbuf *m);`
    - `th`: TCP header reference
    - `seq_start`: TCP data sequence number
    - `tlenp`: TCP data length, should be updated by reassembled block length for the following update of SACK block.
    - `m`: contains TCP data only (no TCP header). It can be NULL.
    - should return saved `th->th_flags & TH_FIN` when reassembled data is appended to the socket buffer.
  - `bool tcp_reass_is_empty(struct tcpcb *tp);`
    - return true if there is no segment in the reassembly queue.

TCP reassembly is a part of received packet processing and it needs to call following functions.
  - `struct tcpcb * tcp_drop(struct tcpcb *, int res);`
  - `struct tcpcb * tcp_close(struct tcpcb *);`

### _**TCP Tunables**_
TCP tunable variables are collected in `struct tcp_tune`. You should instantiate and update it by initial value or given value from the user. It is defined at `netinet/tcp_var.h`.

`trex-core` should extend the `struct tcp_tune` for the additional tunable variables to handling IP features and others. But in the TCP stack, it refres the tunables in the `struct tcp_tune` only. It is assumed that the link is in the `(struct tcpcb *)tp->t_tune`. So it should be initialized by the updated intance.

`trex-core` has `tcp_no_delay` and `tcp_no_delay_counter` to be supported by FreeBSD TCP stack.
  - `tcp_no_delay`
    - nagle (0x1): `tp->t_flags |= TF_NODELAY`
    - push (0x2): `tp->t_flags &= ~TF_NOPUSH` (TF_NODELAY_PUSH is not supported)
  - `tcp_no_delay_counter`: can handled by the following callback function
    - `bool tcp_check_no_delay(struct tcpcb *, int bytes);`
        - `bytes`: bytes to be accumulated. reset accumulated bytes if bytes < 0.

### _**TCP Statistics**_
TCP counters are accumulated at `struct tcpstat`. The counters will be updated by following macro functions. They are defined at `netinet/tcp_var.h`. `(struct tcpcb *)tp` should be accessible in the scope.
  - `TCPSTAT_ADD(name,val)`
  - `TCPSTAT_INC(name)`

`trex-core` needs to update the TCP counters for `tg_id` and `profile`. To support them updated, `(struct tcpcb *)tp` has two `struct tcpstat` references, `tp->t_stat` and `tp->t_stat_ex`.

### _**`struct tcpcb`**_
`tcpcb`(TCP control block) is the main data structure for handling TCP functions per flow. To use it properly, it should be initialized by `tcp_inittcpcb()` at first.
  - `void tcp_inittcpcb(struct tcpcb *tp, struct tcpcb_param *param);`
    - `param`: parameters to intialize tcpcb structure. see `struct tcpcb_param` in `netinet/tcp_var.h`
      - `struct tcp_function_block *fb`: for the next BBR integration, `NULL` for default function block.
      - `struct cc_algo *cc_algo`: selected congestion algorithm. (`newreno_cc_algo` or `cubic_cc_algo`)
      - `struct tcp_tune *tune`: for the reference of tunable variables in `tp`
      - `struct tcpstat *stat`: to collect TCP statistics.
      - `struct tcpstat *stat_ex`: to collect another TCP statistics.
      - `uint32_t *tcp_ticks`: a pointer to current TCP ticks.

Since memory allocation may happen during initialization, `tcp_discardcb()` should be called before release it.
  - `void tcp_discardcb(struct tcpcb *tp);`

You should implement `struct socket * tcp_getsocket(struct tcpcb *tp);` to provided a `tp` related user socket. The socket should be initialized before you use it.
  - `so_options`: set SO_DEBUG to enable `tcp_trace()` output. (In `trex-core`, it is the same as US_SO_DEBUG)
  - `so_rcv.sb_hiwat`,`so_snd.sb_hiwat`: set socket buffer size


### _**`Miscellaneous`**_

The initial sequence number is needed send first SYN and SYN+ACK packet. Since `trex-core` already has the generation mechanism, new TCP stack integration declares following interface function.
  - `uint32_t tcp_new_isn(struct tcpcb *);`
