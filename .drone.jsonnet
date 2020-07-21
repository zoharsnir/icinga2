local Pipeline(os) = {
  kind: "pipeline",
  type: "kubernetes",
  name: os,
  metadata: {
    namespace: drone,
  }
  steps: [
    {
      name: "build",
      image: "registry.icinga.com/build-docker/" + os,
      commands: [
        "./.drone.sh",
      ]
    }
  ]
};

[
  Pipeline("debian/buster"),
  Pipeline("ubuntu/focal"),
  Pipeline("centos/7"),
]
