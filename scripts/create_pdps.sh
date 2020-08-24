#! /bin/bash

# GRPC address of EDA
EDA_GRPC_ADDRESS=":9111"
PDP_TUNNEL_REM_IP=""
PDP_TUNNEL_LOC_IP=""

DATA_FILE="/etc/gtpu-tunnels.csv"

# Creates a PDP context and set the PDP_CTX_ID variable as a result.
create_pdp_ctx() {
    local ENDPOINT_IP=$1
    local LOCAL_TEID=$2
    local REMOTE_TEID=$3

    /opt/eda-tools/bin/eda-grpc-cli CreatePDP --server.address $EDA_GRPC_ADDRESS \
        "{\"pdp_ctx_attributes\": {\"endpoint\":{\"ip_address\":\"$ENDPOINT_IP\", \
        \"ip_address_version\":\"IPV4\"}, \"tunnel\":{\"local_ip\":\"$PDP_TUNNEL_REM_IP\", \
        \"remote_ip\":\"$PDP_TUNNEL_LOC_IP\", \"local_teid\":$REMOTE_TEID, \"remote_teid\":$LOCAL_TEID}}}"
}

echo "# endpoint-ip,src-teid,dst-teid" > $DATA_FILE

i0=16
teid=0
for i1 in {0..0}
do
    for i2 in {0..0}
    do
        for i3 in {0..255}
        do
	    IP="$i0.$i1.$i2.$i3"
            create_pdp_ctx $IP $teid $teid
	    echo "$IP,$teid,$teid" >> $DATA_FILE

	    teid=$(( $teid + 1 ))
	done
    done
done
