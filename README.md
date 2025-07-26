# AI Blueprint Assistant

A powerful Unreal Engine plugin that leverages Google's Gemini AI to automatically summarize blueprint graphs and selected nodes, helping development teams quickly understand complex blueprint logic without manually scanning through every node.

## Features

- **Blueprint Graph Summarization**: Get AI-powered summaries of entire blueprint graphs
- **Selective Node Analysis**: Analyze specific nodes or node groups for targeted insights
- **Team Collaboration**: Help team members quickly review and understand each other's blueprint work
- **Complex Graph Navigation**: Make sense of large, complex blueprint graphs at a glance

## Requirements

- Unreal Engine (compatible version)
- Google Gemini Flash 2.5 API key
- C++ development environment (if building from source)

## Installation

1. **Get a Gemini API Key**
   - Visit [Google AI Studio](https://aistudio.google.com/)
   - Create a new API key for Gemini Flash 2.5

2. **Install the Plugin**
   - Download or clone this repository
   - Copy the entire plugin folder to your Unreal Engine project's `Plugins` directory
   - If the `Plugins` folder doesn't exist in your project root, create it

3. **Enable the Plugin**
   - Open your Unreal Engine project
   - Go to `Edit` → `Plugins`
   - Search for "AI Blueprint Assistant"
   - Check the box to enable the plugin
   - Restart Unreal Engine when prompted

4. **Configure API Key**
   - The plugin will prompt you for your Gemini API key on first use

## Usage

1. **Open the Assistant**
   - In Unreal Engine, go to `Window` → `AI Blueprint Assistant`
   - The AI Blueprint Assistant panel will open

2. **Analyze Blueprints**
   - Open any blueprint graph
   - Select specific nodes (optional) or analyze the entire graph
   - Click the analyze button in the AI Blueprint Assistant panel
   - View the AI-generated summary and insights

## Use Cases

- **Code Reviews**: Quickly understand what a blueprint does before reviewing
- **Knowledge Transfer**: Help new team members understand existing blueprint logic
- **Documentation**: Generate quick summaries for blueprint documentation
- **Debugging**: Get high-level understanding of complex graphs when troubleshooting

## Technology Stack

- **Language**: C++
- **Engine**: Unreal Engine
- **AI Service**: Google Gemini Flash 2.5

## Contributing

Contributions are welcome! Please feel free to submit issues, feature requests, or pull requests to help improve the AI Blueprint Assistant.

## License


## Support

If you encounter any issues or have questions:
- Check the [Issues](../../issues) section
- Create a new issue with detailed information about your problem
- Include your Unreal Engine version and plugin version

---

**Note**: This plugin requires an active internet connection and valid Gemini API key to function. API usage charges may apply based on your Google Cloud billing.
