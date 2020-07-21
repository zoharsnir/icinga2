local Pipeline(os) = {
  kind: "pipeline",
  type: "kubernetes",
  name: os,
  metadata: {
    namespace: "drone",
  },
  node_selector: {
    magnum.openstack.org/nodegroup: "build-worker",
  },
  steps: [
    {
      name: "build",
      image: "registry.icinga.com/build-docker/" + os,
      commands: [
        "./.drone.sh",
      ],
      resources: {
        requests: {
          cpu: 3000,
          memory: "3GiB",
        },
        limits: {
          cpu: 3000,
          memory: "3GiB",
        }
      }
    }
  ]
};

[
  Pipeline("debian/buster"),
]
