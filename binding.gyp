{
  "targets": [
    {
      "target_name": "thunderbringer",
      "type": "none",
      "actions": [
        {
          "action_name": "build_via_cmake_js",
          "inputs": [],
          "outputs": ["build/Release/thunderbringer.node"],
          "action": ["node", "scripts/install.js"]
        }
      ]
    }
  ]
}
