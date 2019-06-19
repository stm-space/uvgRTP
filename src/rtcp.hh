#pragma once

#include <map>
#include <thread>
#include <vector>

#include "frame.hh"
#include "socket.hh"
#include "util.hh"

namespace kvz_rtp {

    class connection;

    class rtcp {
    public:
        rtcp();
        rtcp(std::string dst_addr, int dst_port, bool receiver);
        rtcp(std::string dst_addr, int dst_port, int src_port, bool receiver);
        ~rtcp();

        /* start the RTCP runner thread
         *
         * return RTP_OK on success and RTP_MEMORY_ERROR if the allocation fails */
        rtp_error_t start();

        /* End the RTCP session and send RTCP BYE to all participants
         *
         * return RTP_OK on success */
        rtp_error_t terminate();

        /* return true if the connection is still considered active
         * and RTCP transmissions should continue */
        bool active() const;
        
        /* return true if this RTCP instance belongs to an RTP receiver 
         * and a receiver report should be generated, otherwise sender report is generated */
        bool receiver() const;

        /* Generate either RTCP Sender or Receiver report and sent it to all participants 
         * Return RTP_OK on success and RTP_ERROR on error */
        rtp_error_t generate_report();

        /* Handle different kinds of incoming packets 
         *
         * Currently nothing's done with valid packets, at some point an API for
         * querying these repots is implemented
         *
         * Return RTP_OK on success and RTP_ERROR on error */
        rtp_error_t handle_sender_report_packet(kvz_rtp::frame::rtcp_sender_frame *sender_report);
        rtp_error_t handle_receiver_report_packet(kvz_rtp::frame::rtcp_receiver_frame *receiver_report);
        rtp_error_t handle_sdes_packet(kvz_rtp::frame::rtcp_sdes_frame *sdes);
        rtp_error_t handle_bye_packet(kvz_rtp::frame::rtcp_bye_frame *bye);
        rtp_error_t handle_app_packet(kvz_rtp::frame::rtcp_app_frame *app);

        /* Handle incoming RTCP packet (first make sure it's a valid RTCP packet)
         * This function will call one of the above functions internally
         *
         * Return RTP_OK on success and RTP_ERROR on error */
        rtp_error_t handle_incoming_packet(uint8_t *buffer, size_t size);

        /* Send "frame" to all participants
         *
         * These routines will convert all necessary fields to network byte order
         *
         * Return RTP_OK on success
         * Return RTP_INVALID_VALUE if "frame" is in some way invalid
         * Return RTP_SEND_ERROR if sending "frame" did not succeed (see socket.hh for details) */
        rtp_error_t send_sender_report_packet(kvz_rtp::frame::rtcp_sender_frame *frame);
        rtp_error_t send_receiver_report_packet(kvz_rtp::frame::rtcp_receiver_frame *frame);
        rtp_error_t send_sdes_packet(kvz_rtp::frame::rtcp_sdes_frame *frame);
        rtp_error_t send_bye_packet(kvz_rtp::frame::rtcp_bye_frame *frame);
        rtp_error_t send_app_packet(kvz_rtp::frame::rtcp_app_frame *frame);

        /* Return reference to kvz_rtp::socket of the RTCP instance
         * Used by the rtcp_runner to listen to incoming  */
        const kvz_rtp::socket& get_socket() const;

        /* Somebody joined the multicast group the owner of this RTCP instance is part of
         * Add it to RTCP participant list so we can start listening for reports 
         *
         * Return RTP_OK on success and RTP_ERROR on error */
        rtp_error_t add_participant(kvz_rtp::connection *conn);

        /* Functions for updating various RTP sender statistics */
        void sender_inc_processed_bytes(size_t n);
        void sender_inc_overhead_bytes(size_t n);
        void sender_inc_total_bytes(size_t n);
        void sender_inc_processed_pkts(size_t n);

        void receiver_inc_processed_bytes(uint32_t sender_ssrc, size_t n);
        void receiver_inc_overhead_bytes(uint32_t sender_ssrc, size_t n);
        void receiver_inc_total_bytes(uint32_t sender_ssrc, size_t n);
        void receiver_inc_processed_pkts(uint32_t sender_ssrc, size_t n);

    private:
        static void rtcp_runner(rtcp *rtcp);

        /* convert struct timeval format to NTP format */
        uint64_t tv_to_ntp();

        /* when we start the RTCP instance, we don't know what the SSRC of the remote is
         * when an RTP packet is received, we must check if we've already received a packet 
         * from this sender and if not, create new entry to receiver_stats_ map */
        void check_sender(uint32_t ssrc);

        /* Functions for generating different kinds of reports. 
         * These functions will both generate the report and send it 
         *
         * Return RTP_OK on success and RTP_ERROR on error */
        rtp_error_t generate_sender_report();
        rtp_error_t generate_receiver_report();

        std::thread *runner_;
        std::string cname_;
        bool receiver_;

        /* TODO: time_t?? */
        size_t tp_;       /* the last time an RTCP packet was transmitted */
        size_t tc_;       /* the current time */
        size_t tn_;       /* the next scheduled transmission time of an RTCP packet */
        size_t pmembers_; /* the estimated number of session members at the time tn was last recomputed */
        size_t members_;  /* the most current estimate for the number of session members */
        size_t senders_;  /* the most current estimate for the number of senders in the session */

        /* The target RTCP bandwidth, i.e., the total bandwidth
         * that will be used for RTCP packets by all members of this session,
         * in octets per second.  This will be a specified fraction of the
         * "session bandwidth" parameter supplied to the application at startup. */
        size_t rtcp_bandwidth_;

        /* Flag that is true if the application has sent data since
         * the 2nd previous RTCP report was transmitted. */
        bool we_sent_;

        /* The average compound RTCP packet size, in octets,
         * over all RTCP packets sent and received by this participant.  The
         * size includes lower-layer transport and network protocol headers
         * (e.g., UDP and IP) as explained in Section 6.2 */
        size_t avg_rtcp_pkt_pize_;

        /* Flag that is true if the application has not yet sent an RTCP packet. */
        bool initial_;

        /* Flag that is true if the connection is still considered open.
         * When clients decided to leave, he calls rtcp->terminate()
         * which stops the rtcp runner and RTCP BYE message to all participants */
        bool active_;

        /* send_addr_:send_port_ is used for sending reports to remote
         * recv_port_ is used for receiving reports */
        std::string send_addr_;
        int send_port_;
        int recv_port_;

        kvz_rtp::socket socket_;

        /* TODO: is there any use for this?? */
        std::vector<kvz_rtp::connection *> participant_;

        struct statistics {
            size_t processed_bytes;
            size_t overhead_bytes;
            size_t total_bytes;
            size_t processed_pkts;
            size_t dropped_pkts;
        };

        /* statistics for RTCP Sender and Receiver Reports */
        struct statistics sender_stats_;

        std::map<uint32_t, struct statistics *> receiver_stats_;
        size_t num_receivers_;
    };
};