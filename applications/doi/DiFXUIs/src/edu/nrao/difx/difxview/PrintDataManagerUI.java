/*
 * To change this template, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * PrintDataManagerUI.java
 *
 * Created on Jun 13, 2009, 12:24:25 PM
 */

package edu.nrao.difx.difxview;

import java.awt.print.PrinterException;
import java.awt.print.PrinterJob;

/**
 *
 * @author mguerra
 */
public class PrintDataManagerUI extends javax.swing.JFrame {

    static private final String newline = "\n";

    /** Creates new form PrintDataManagerUI */
    public PrintDataManagerUI() {
        initComponents();
    }

    /** Creates new form PrintDataManagerUI */
    public PrintDataManagerUI(String text) {
        initComponents();
        textArea.setText(text);
        textArea.setTabSize(4);
    }

    public void append(String text)
    {
      textArea.append(text + newline);
    }

    /** This method is called from within the constructor to
     * initialize the form.
     * WARNING: Do NOT modify this code. The content of this method is
     * always regenerated by the Form Editor.
     */
    @SuppressWarnings("unchecked")
   // <editor-fold defaultstate="collapsed" desc="Generated Code">//GEN-BEGIN:initComponents
   private void initComponents() {

      jScrollPane1 = new javax.swing.JScrollPane();
      textArea = new javax.swing.JTextArea();
      jMenuBar1 = new javax.swing.JMenuBar();
      jMenu1 = new javax.swing.JMenu();
      jMenuItem1 = new javax.swing.JMenuItem();

      setDefaultCloseOperation(javax.swing.WindowConstants.DISPOSE_ON_CLOSE);

      textArea.setColumns(20);
      textArea.setFont(textArea.getFont().deriveFont((float)9));
      jScrollPane1.setViewportView(textArea);

      jMenu1.setText("File");

      jMenuItem1.setText("Print");
      jMenuItem1.addActionListener(new java.awt.event.ActionListener() {
         public void actionPerformed(java.awt.event.ActionEvent evt) {
            jMenuItem1ActionPerformed(evt);
         }
      });
      jMenu1.add(jMenuItem1);

      jMenuBar1.add(jMenu1);

      setJMenuBar(jMenuBar1);

      javax.swing.GroupLayout layout = new javax.swing.GroupLayout(getContentPane());
      getContentPane().setLayout(layout);
      layout.setHorizontalGroup(
         layout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
         .addComponent(jScrollPane1, javax.swing.GroupLayout.DEFAULT_SIZE, 609, Short.MAX_VALUE)
      );
      layout.setVerticalGroup(
         layout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
         .addComponent(jScrollPane1, javax.swing.GroupLayout.DEFAULT_SIZE, 279, Short.MAX_VALUE)
      );

      pack();
   }// </editor-fold>//GEN-END:initComponents

    private void jMenuItem1ActionPerformed(java.awt.event.ActionEvent evt)//GEN-FIRST:event_jMenuItem1ActionPerformed
    {//GEN-HEADEREND:event_jMenuItem1ActionPerformed

      PrinterJob printJob = PrinterJob.getPrinterJob();
      printJob.setPrintable(textArea.getPrintable(null, null));
      if (printJob.printDialog())
      {
         try
         {
            printJob.print();
         }
         catch (PrinterException pe)
         {
            System.out.println("Error printing: " + pe);
         }
      }

    }//GEN-LAST:event_jMenuItem1ActionPerformed

    /**
    * @param args the command line arguments
    */
    public static void main(String args[]) {
        java.awt.EventQueue.invokeLater(new Runnable() {
            public void run() {
                new PrintDataManagerUI("BogusText").setVisible(true);
            }
        });
    }

   // Variables declaration - do not modify//GEN-BEGIN:variables
   private javax.swing.JMenu jMenu1;
   private javax.swing.JMenuBar jMenuBar1;
   private javax.swing.JMenuItem jMenuItem1;
   private javax.swing.JScrollPane jScrollPane1;
   private javax.swing.JTextArea textArea;
   // End of variables declaration//GEN-END:variables

}