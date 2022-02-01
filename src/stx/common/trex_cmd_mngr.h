#ifndef __TREX_CMD_MNGR_H__
#define __TREX_CMD_MNGR_H__

#include <mutex>
#include <json/json.h>

typedef enum {
    KILL_REQ = 0,
    POPEN_REQ = 1,
    NEW_CLIENT_REQ = 2
} request_type_t;

typedef enum{
    NONE,
    ZMQ_ERROR,
    PARSING_ERROR
}error_type_t;

typedef enum {
    FIELD_TYPE_STR,
    FIELD_TYPE_UINT32
} field_type_t;

class CCmdsMngr {
public:
    CCmdsMngr();
    ~CCmdsMngr();

    /**
     * convert error_type_t value to string.
     *
     * @param error
     *   an error_type_t value
     * @return error_string
     *   The error in string.
     */
    std::string error_type_to_string(error_type_t error);


    /**
     * Execute a popen command using the python cmds server.
     * Each command are being sent, over ZMQ channel, to the python server, that execute the command
     * and sends the results.
     *
     * @param cmd
     *   The command to be excecuted
     *
     * @param output
     *   The output of the child process of the python cmds server.
     *
     * @param error
     *   a Non popen error that occurred while communicating with the python cmds server.
     *
     * @param error_msg
     *   an error description in case of non popen error.
     *
     * @return int
     *  0 : in case of success.
     * -1 : in case of any failure.
     */
    int popen_general(const std::string &cmd, std::string &output, error_type_t& error, std::string &error_msg);

private:
    void send_request(Json::Value &req, Json::Value &resp);
    void verify_field(Json::Value &resp, std::string field_name, field_type_t type);

private:
    void*                   m_zmq_ctx;
    void*                   m_zmq_cmds_socket;
    std::mutex              m_cmds_mutex;
};


#endif