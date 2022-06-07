const functions = require("firebase-functions");
const axios = require("axios");
exports.fire = functions.database.ref("/Nodes-Status/{weid}/{nodesid}")
    .onUpdate((change, context)=> {
      const fires = change.after.val();
      const firesb = change.before.val();
      const firedata = fires.fire_Status;
      const nodeid = context.params.nodesid;
      const weid = context.params.weid;
      const payload = {
        "NodeId": nodeid,
        "GatewayID": weid,
        "fire_status": firedata,
      };
      if (fires != firesb) {
        if (firedata == 1) {
          console.log(`fire on node ${nodeid}`);
          console.log(`fire on node ${weid}`);
          return axios.post("https://wildeye-the-perfact-eye.herokuapp.com/firedetect", payload)
              .then(function() {
                console.log("Successfull");
              })
              .catch(function() {
                console.log("Some error occures");
              });
        } else {
          console.log(`fire is vanished from ${nodeid} in ${weid}`);
          return axios.post("https://wildeye-the-perfact-eye.herokuapp.com/firedetect", payload)
              .then(function() {
                console.log("Successfull");
              })
              .catch(function() {
                console.log("Some error occures");
              });
        }
      }
    });