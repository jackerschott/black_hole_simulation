#define COMPILE_GTK true
#if COMPILE_GTK

#include <gtk/gtk.h>
#include "render.h"

GtkWidget *window_main;
GtkImage *img_main;
GtkAdjustment *adj_rg;
GtkAdjustment *adj_rs;
GtkWidget *btn_render;
GtkAdjustment *adj_nParticles;
GtkAdjustment *adj_rr;
GtkAdjustment *adj_rtheta;
GtkAdjustment *adj_rphi;
GtkAdjustment *adj_rdr;
GtkAdjustment *adj_rdtheta;
GtkAdjustment *adj_rdphi;
GtkWidget *btn_rcolors;
GtkWidget *btn_ring;

GtkWidget *window_colorPalette;
GtkWidget *cbt_select;
GtkWidget *btn_delete;
GtkAdjustment *adj_prob;
GtkWidget *ccwidget_color;
GtkWidget *btn_close;

uint w = 920;
uint h = 690;
byte* pixels;
bool rendering = false;
byte activeColor = 1;
uint nColors = 1;
probColor *colorPalette = new probColor[nColors];

void on_window_main_destroy() {
  g_print("Exit\n");
  gtk_main_quit();
}

void on_key_press(GtkWidget *widget, GdkEventKey *event) {
  switch (event->keyval) {
    case GDK_KEY_w: case GDK_KEY_Up:
      g_print("forward\n");
      break;
    case GDK_KEY_a: case GDK_KEY_Left:
      g_print("left\n");
      break;
    case GDK_KEY_s: case GDK_KEY_Down:
      g_print("backward\n");
      break;
    case GDK_KEY_d: case GDK_KEY_Right:
      g_print("right\n");
      break;
    default:
      g_print("else\n");
  }
}

void on_key_release(GtkWidget *widget, GdkEventKey *event) {
  switch (event->keyval) {
    case GDK_KEY_w: case GDK_KEY_Up:
      g_print("stop forward\n");
      break;
    case GDK_KEY_a: case GDK_KEY_Left:
      g_print("stop left\n");
      break;
    case GDK_KEY_s: case GDK_KEY_Down:
      g_print("stop backward\n");
      break;
    case GDK_KEY_d: case GDK_KEY_Right:
      g_print("stop right\n");
      break;
    default:
      g_print("released else\n");
    }
}

void on_adj_rx_changed() {
  double rg_value = gtk_adjustment_get_value(adj_rg);
  double rs_value = gtk_adjustment_get_value(adj_rs);
  double rr_value = gtk_adjustment_get_value(adj_rr);
  gtk_adjustment_set_upper(adj_rs, rg_value);
  gtk_adjustment_set_upper(adj_rr, rg_value);
  gtk_adjustment_set_lower(adj_rg, rs_value);
  gtk_adjustment_set_lower(adj_rr, rs_value);
  if (rr_value < rs_value) {
    gtk_adjustment_set_value(adj_rr, rs_value);
  }
  if (rr_value > rg_value) {
    gtk_adjustment_set_value(adj_rr, rg_value);
  }
}

void on_btn_render_clicked() {
  char* btn_lbl;
  if (rendering) {
    gtk_button_set_label(GTK_BUTTON(btn_render), "Render");
    rendering = false;
  }
  else {
    gtk_button_set_label(GTK_BUTTON(btn_render), "Reset");
    rendering = true;
  }
}

void on_btn_rcolors_clicked() {
  gtk_widget_show(window_colorPalette);
}

void save_color(byte index) {
  index--;
  GdkRGBA gcolor;
  gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(ccwidget_color), &gcolor);
  colorPalette[index].r = gcolor.red;
  colorPalette[index].g = gcolor.green;
  colorPalette[index].b = gcolor.blue;
  colorPalette[index].p = gtk_adjustment_get_value(adj_prob);
}

void load_color(byte index) {
  index--;
  GdkRGBA gcolor;
  gcolor.red = colorPalette[index].r;
  gcolor.green = colorPalette[index].g;
  gcolor.blue = colorPalette[index].b;
  gcolor.alpha = 1.0;
  gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(ccwidget_color), &gcolor);
  gtk_adjustment_set_value(adj_prob, colorPalette[index].p);
}

void on_btn_ring_clicked() {
  g_print("Creating Ring...\n");
  uint nParticles = (uint)gtk_adjustment_get_value(adj_nParticles);
  double rr = gtk_adjustment_get_value(adj_rr);
  double rtheta = gtk_adjustment_get_value(adj_rtheta);
  double rphi = gtk_adjustment_get_value(adj_rphi);
  double rdr = gtk_adjustment_get_value(adj_rdr);
  double rdtheta = gtk_adjustment_get_value(adj_rdtheta);
  double rdphi = gtk_adjustment_get_value(adj_rdphi);
  perspectiveCamera camera;
  camera.pos = { 15.0, -30.0, 15.0 };
  camera.lookDir = { -1.0, 2.0, -1.0 };
  camera.upDir = { 0.0, 0.0, 1.0 };
  camera.fov = 60.0;
  setCamera(camera);
  vector rn = { sin(rtheta) * cos(rphi), sin(rtheta) * sin(rphi), cos(rtheta) };
  createParticleRing(nParticles, rr, rn, rdr, rdtheta, rdphi, 5, colorPalette);
  delete[] pixels;
  renderConfig(w, h);
  pixels = render();
  GBytes* gPixels = g_bytes_new(pixels, w * h * bpp / 8);
  GdkPixbuf* pixbuf = gdk_pixbuf_new_from_bytes(gPixels, GDK_COLORSPACE_RGB, false, 8, w, h, w * bpp / 8);
  gtk_image_set_from_pixbuf(img_main, pixbuf);
  g_print("done creating Ring\n");
}

void on_cbt_select_changed() {
  save_color(activeColor);
  activeColor = gtk_combo_box_get_active(GTK_COMBO_BOX(cbt_select));
  if (activeColor == 0) {
    probColor *newColorPalette = new probColor[nColors + 1];
    for (uint i = 0; i < nColors; i++) {
      newColorPalette[i] = colorPalette[i];
    }
    newColorPalette[nColors] = { 1.0, 1.0, 1.0, 0.0 }; //random here
    delete[] colorPalette;
    colorPalette = newColorPalette;
    activeColor = nColors++;
    load_color(activeColor);
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(cbt_select), ("Color " + std::to_string(activeColor)).c_str());
    gtk_combo_box_set_active(GTK_COMBO_BOX(cbt_select), activeColor);
  }
  else {
    load_color(activeColor);
  }
}

void on_btn_close_clicked() {
  save_color(activeColor);
  gtk_widget_hide(window_colorPalette);
}

int main(int argc, char *argv[]) {
  GtkBuilder *builder;
  GError *err = NULL;
  gtk_init(&argc, &argv);
  builder = gtk_builder_new();
  gtk_builder_add_from_file(builder, "gui/window_main.glade", &err);
  gtk_builder_add_from_file(builder, "gui/window_colorPalette.glade", &err);

  window_main = GTK_WIDGET(gtk_builder_get_object(builder, "window_main"));
  img_main = GTK_IMAGE(gtk_builder_get_object(builder, "img_main"));
  adj_rg = GTK_ADJUSTMENT(gtk_builder_get_object(builder, "adj_rg"));
  adj_rs = GTK_ADJUSTMENT(gtk_builder_get_object(builder, "adj_rs"));
  btn_render = GTK_WIDGET(gtk_builder_get_object(builder, "btn_render"));
  adj_nParticles = GTK_ADJUSTMENT(gtk_builder_get_object(builder, "adj_nParticles"));
  adj_rr = GTK_ADJUSTMENT(gtk_builder_get_object(builder, "adj_rr"));
  adj_rtheta = GTK_ADJUSTMENT(gtk_builder_get_object(builder, "adj_rtheta"));
  adj_rphi = GTK_ADJUSTMENT(gtk_builder_get_object(builder, "adj_rphi"));
  adj_rdr = GTK_ADJUSTMENT(gtk_builder_get_object(builder, "adj_rdr"));
  adj_rdtheta = GTK_ADJUSTMENT(gtk_builder_get_object(builder, "adj_rdtheta"));
  adj_rdphi = GTK_ADJUSTMENT(gtk_builder_get_object(builder, "adj_rdphi"));
  btn_rcolors = GTK_WIDGET(gtk_builder_get_object(builder, "btn_rcolors"));
  btn_ring = GTK_WIDGET(gtk_builder_get_object(builder, "btn_ring"));
  g_signal_connect(window_main, "destroy", G_CALLBACK(on_window_main_destroy), NULL);
  g_signal_connect(window_main, "key-press-event", G_CALLBACK(on_key_press), NULL);
  g_signal_connect(window_main, "key-release-event", G_CALLBACK(on_key_release), NULL);
  g_signal_connect(adj_rg, "value-changed", G_CALLBACK(on_adj_rx_changed), NULL);
  g_signal_connect(adj_rs, "value-changed", G_CALLBACK(on_adj_rx_changed), NULL);
  g_signal_connect(adj_rr, "value-changed", G_CALLBACK(on_adj_rx_changed), NULL);
  g_signal_connect(btn_render, "clicked", G_CALLBACK(on_btn_render_clicked), NULL);
  g_signal_connect(btn_rcolors, "clicked", G_CALLBACK(on_btn_rcolors_clicked), NULL);
  g_signal_connect(btn_ring, "clicked", G_CALLBACK(on_btn_ring_clicked), NULL);

  window_colorPalette = GTK_WIDGET(gtk_builder_get_object(builder, "window_colorPalette"));
  cbt_select = GTK_WIDGET(gtk_builder_get_object(builder, "cbt_select"));
  btn_delete = GTK_WIDGET(gtk_builder_get_object(builder, "btn_delete"));
  adj_prob = GTK_ADJUSTMENT(gtk_builder_get_object(builder, "adj_prob"));
  ccwidget_color = GTK_WIDGET(gtk_builder_get_object(builder, "ccwidget_color"));
  btn_close = GTK_WIDGET(gtk_builder_get_object(builder, "btn_close"));
  gtk_window_set_transient_for(GTK_WINDOW(window_colorPalette), GTK_WINDOW(window_main));
  g_signal_connect(window_colorPalette, "delete-event", G_CALLBACK(on_btn_close_clicked), NULL);
  g_signal_connect(cbt_select, "changed", G_CALLBACK(on_cbt_select_changed), NULL);
  //g_signal_connect(btn_delete, "clicked", G_CALLBACK(on_btn_delete_clicked), NULL);
  //g_signal_connect(adj_prob, "value-changed", G_CALLBACK(on_adj_prob_changed), NULL);
  g_signal_connect(btn_close, "clicked", G_CALLBACK(on_btn_close_clicked), NULL);

  colorPalette[0] = {1.0, 1.0, 1.0, 1.0};
  pixels = new byte[w * h * bpp / 8];
  for (uint i = 0; i < w * h * bpp / 8; i++) {
    pixels[i] = 0;
  }
  GBytes* gPixels = g_bytes_new(pixels, w * h * bpp / 8);
  GdkPixbuf* pixbuf = gdk_pixbuf_new_from_bytes(gPixels, GDK_COLORSPACE_RGB, false, 8, w, h, w * bpp / 8);
  gtk_image_set_from_pixbuf(img_main, pixbuf);
  renderInit(1.0, 10.0);

  gtk_widget_show(window_main);
  gtk_main();
  return 0;
}

#else

#include <cmath>
#include <iostream>
#include <fstream>
#include <random>
#include "render.h"

#define _USE_MATH_DEFINES

int main(int argc, char *argv[]) {
  uint w = 920;
  uint h = 690;
  double rs = 1.0;
  double gr = 10.0 * rs;

  const uint nParticles = 5000;

  perspectiveCamera camera;
  camera.pos = { 15.0, -30.0, 15.0 };
  camera.lookDir = { -1.0, 2.0, -1.0 };
  camera.upDir = { 0.0, 0.0, 1.0 };
  camera.fov = 60.0;

  std::pair<color, double> *colorPalette = new std::pair<color, double>[nColors];
  for (uint i = 0; i < nColors; i++) {
    colorPalette[i] = { { (byte)(rand() % 256), (byte)(rand() % 256), (byte)(rand() % 256) }, 1.0 };
  }
  renderInit(rs, rg);
  createParticleRing(nParticles, 5.0 * rs, { 0.0, -1.0, 1.0 }, 0.1 * rs, 0.1, 0.1, nColors, colorPalette);
  setCamera(camera);
  renderConfig(w, h);
  byte* pixels = render();

  //std::ofstream out;
  //out.open("E:\\Zwischenspeicher\\out");
  //for (int i = 0; i < w * h * bpp / 8; i++) {
  //  out << pixels[i];
  //}
  //out.close();

  return 0;
}

#endif
