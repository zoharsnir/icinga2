local Pipeline(os) = {
  kind: "pipeline",
  type: "kubernetes",
  name: os,
  metadata: {
    namespace: "drone"
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
          cpu: 2000,
          memory: 2GiB
        }
      }
    }
  ]
};

[
  Pipeline("debian/buster"),
  Pipeline("debian/stretch"),
  Pipeline("ubuntu/focal"),
  Pipeline("ubuntu/eoan"),
]
