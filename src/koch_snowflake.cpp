
/***************************************************************************
 * Lab assignment 1 - Alexander Mennborg
 * Koch Snowflake - https://en.wikipedia.org/wiki/Koch_snowflake
 ***************************************************************************/


void initialize_scene() {

}


void update_and_render_scene() {
    glClearColor(0.3f, 0.5f, 0.8f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    static bool show = true;
    static int recursion_depth = 1;
    
    ImGui::Begin("Koch Snowflake", &show, ImGuiWindowFlags_NoSavedSettings);

    ImGui::Text("Recursion Depth");
    ImGui::SliderInt("", &recursion_depth, 1, 7);
    ImGui::End();

}
